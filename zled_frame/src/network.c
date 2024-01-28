/*
 * Copyright (c) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network.h"

#include <zephyr/logging/log.h>
#define LOG_LEVEL LOG_LEVEL_DBG
LOG_MODULE_REGISTER(networking); // TODO: how does it work?

#if defined(CONFIG_NET_IPV4)
K_THREAD_STACK_ARRAY_DEFINE(tcp4_handler_stack, CONFIG_NET_SAMPLE_NUM_HANDLERS,
							STACK_SIZE);
static struct k_thread tcp4_handler_thread[CONFIG_NET_SAMPLE_NUM_HANDLERS];
static k_tid_t tcp4_handler_tid[CONFIG_NET_SAMPLE_NUM_HANDLERS];
#endif

#if defined(CONFIG_NET_IPV6)
// TODO: somehow in the original sample IPv6 is mandatory - otherwise we get a crash upon second HTTP request?
K_THREAD_STACK_ARRAY_DEFINE(tcp6_handler_stack, CONFIG_NET_SAMPLE_NUM_HANDLERS,
							STACK_SIZE);
static struct k_thread tcp6_handler_thread[CONFIG_NET_SAMPLE_NUM_HANDLERS];
static k_tid_t tcp6_handler_tid[CONFIG_NET_SAMPLE_NUM_HANDLERS];
#endif

static struct net_mgmt_event_callback mgmt_cb;
static bool connected;
K_SEM_DEFINE(run_app, 0, 1);
K_SEM_DEFINE(quit_lock, 0, 1);
static bool running_status;
static bool want_to_quit;

static int tcp4_listen_sock;
static int tcp4_accepted[CONFIG_NET_SAMPLE_NUM_HANDLERS];

static int tcp6_listen_sock;
static int tcp6_accepted[CONFIG_NET_SAMPLE_NUM_HANDLERS];

static void process_tcp4(void);
static void process_tcp6(void);

K_THREAD_DEFINE(tcp4_thread_id, STACK_SIZE,
				process_tcp4, NULL, NULL, NULL,
				THREAD_PRIORITY, 0, -1);

K_THREAD_DEFINE(tcp6_thread_id, STACK_SIZE,
				process_tcp6, NULL, NULL, NULL,
				THREAD_PRIORITY, 0, -1);

#define RECEIVE_BUF_LEN 128

static endpoint_t valid_endpoints[NUMBER_OF_ENDPOINTS] = {
	{"/", GET}, // TODO :add handling for favicon lol
	{"/api/image", POST},
};

#define MAX_IMAGE_SIZE (16 * 16 * 3 * sizeof(uint8_t))
static char received_image[MAX_IMAGE_SIZE];
static size_t rcv_img_offset;

void event_handler(struct net_mgmt_event_callback *cb,
				   uint32_t mgmt_event, struct net_if *iface)
{
	if ((mgmt_event & EVENT_MASK) != mgmt_event)
	{
		return;
	}

	if (mgmt_event == NET_EVENT_L4_CONNECTED)
	{
		LOG_INF("Network connected");

		connected = true;
		k_sem_give(&run_app);

		return;
	}

	if (mgmt_event == NET_EVENT_L4_DISCONNECTED)
	{
		if (connected == false)
		{
			LOG_INF("Waiting network to be connected");
		}
		else
		{
			LOG_INF("Network disconnected");
			connected = false;
		}

		k_sem_reset(&run_app);

		return;
	}
}

static ssize_t sendall(int sock, const void *buf, size_t len)
{
	while (len)
	{
		ssize_t out_len = send(sock, buf, len, 0);

		if (out_len < 0)
		{
			return out_len;
		}

		buf = (const char *)buf + out_len;
		len -= out_len;
	}

	return 0;
}

static int setup(int *sock, struct sockaddr *bind_addr,
				 socklen_t bind_addrlen)
{
	int ret;

	*sock = socket(bind_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (*sock < 0)
	{
		LOG_ERR("Failed to create TCP socket: %d", errno);
		return -errno;
	}

	ret = bind(*sock, bind_addr, bind_addrlen);
	if (ret < 0)
	{
		LOG_ERR("Failed to bind TCP socket %d", errno);
		return -errno;
	}

	ret = listen(*sock, MAX_CLIENT_QUEUE);
	if (ret < 0)
	{
		LOG_ERR("Failed to listen on TCP socket %d", errno);
		ret = -errno;
	}

	return ret;
}

// add the method + endpoint choosing method:

// TODO: endpoint choosing done properly - but do this later - first check the endpoint - integrate to my code and then build on top of that
//
// // TODO :how to handle multipackets

static int parse_header(char *buf, int buf_size, char *endpoint, int endpoint_size, method_t *method)
{
	char *ptr = buf, *delim_pos = NULL;
	int len = 0;
	if (strstr(ptr, "GET"))
	{
		*method = GET;
		ptr += 4; // +1 for whitespace
	}
	else if (strstr(ptr, "POST"))
	{
		*method = POST;
		ptr += 5;
	}
	else
	{
		LOG_ERR("Unknown method found!");
		return -1;
	}

	delim_pos = strchr(ptr, ' ');
	len = delim_pos - ptr;
	if (len + 1 > endpoint_size)
	{
		LOG_ERR("Too long endpoint name! Max allowed: %d", endpoint_size);
		return -1;
	}

	strncpy(endpoint, ptr, len);
	*(endpoint + len) = '\0'; // null-terminate it

	LOG_DBG("Found method of type: %d with endpoint name: %s", *method, endpoint);

	return 0;
}

static int handle_endpoint(int client, char *endpoint_buf, method_t method, char *buf, int buf_len)
{
	// GET /
	if (strncmp(valid_endpoints[0].name, endpoint_buf, strlen(endpoint_buf)) == 0)
	{
		// here we don't need to check any return values and can directly return the page to render
		LOG_ERR("Sending the data to the client!");
		(void)sendall(client, content, sizeof(content));
		return 0;
	}
	else if (strncmp(valid_endpoints[1].name, endpoint_buf, strlen(endpoint_buf)) == 0)
	{
		LOG_ERR("Handling POST /api/image");

		// for now this will inform that we handled all
		if (rcv_img_offset + buf_len >= MAX_IMAGE_SIZE)
		{
			LOG_ERR("Stop accepting - Trying to send more data than Image can accept.");
			// TODO: also skip the header part and only get the raw data
			// TODO: where should it be placed?
			rcv_img_offset = 0;
			return 0;
		}

		LOG_ERR("Position: %d", rcv_img_offset);

		strncpy(received_image + rcv_img_offset, buf, buf_len);

		rcv_img_offset += buf_len;
		return -1;
	}

	/* TODO: when to use those
		if (strstr(buf, "\r\n\r\nOK")) {
			LOG_ERR("Received OK");
			running_status = true;
			want_to_quit = true;
			k_sem_give(&quit_lock);
			break;
		} else if (strstr(buf, "\r\n\r\nFAIL")) {
			LOG_ERR("Received FAIL");
			running_status = false;
			want_to_quit = true;
			k_sem_give(&quit_lock);
			break;
		}
	*/
	LOG_ERR("Unknown endpoint and header!");
	return 0;
}

static void client_conn_handler(void *ptr1, void *ptr2, void *ptr3)
{
	ARG_UNUSED(ptr1);
	int *sock = ptr2;
	k_tid_t *in_use = ptr3;
	int client;
	int received;
	int ret;
	char buf[RECEIVE_BUF_LEN];

	// my things added for handling of the endpoints - TODO: refactor it later to be more composable
	char endpoint_buf[20];
	bool parsed_header = false;
	method_t method;

	client = *sock;

	do
	{
		received = recv(client, buf, sizeof(buf), 0);

		if (received == 0)
		{
			/* Connection closed */
			LOG_DBG("[%d] Connection closed by peer", client);
			break;
		}
		else if (received < 0)
		{
			/* Socket error */
			ret = -errno;
			LOG_ERR("[%d] Connection error %d", client, ret);
			break;
		}

		LOG_DBG("[%d] Received data: %.*s", client, received, buf);

		if (!parsed_header)
		{
			if (parse_header(buf, sizeof(buf), endpoint_buf, sizeof(endpoint_buf), &method) != 0)
			{
				LOG_ERR("[%d] Could not parse header", client);
				break;
			}
			parsed_header = true;
		} // TODO: add sending the data to the proper endpoint later

		/* Note that something like this strstr() check should *NOT*
		 * be used in production code. This is done like this just
		 * for this sample application to keep things simple.
		 *
		 * We are assuming here that the full HTTP request is received
		 * in one TCP segment which in real life might not.
		 */
		// if (strstr(buf, "\r\n\r\n")) { // TODO: how to check it received all data though?
		//     LOG_ERR("END OF PACKET RECEIVED");
		//	break;
		// }
		if (handle_endpoint(client, endpoint_buf, method, buf, RECEIVE_BUF_LEN) == 0)
		{
			LOG_ERR("Handled the endpoint - exiting.");
			break;
		}
	} while (true);

	(void)close(client);

	*sock = -1;
	*in_use = NULL;
}

static int get_free_slot(int *accepted)
{
	int i;

	for (i = 0; i < CONFIG_NET_SAMPLE_NUM_HANDLERS; i++)
	{
		if (accepted[i] < 0)
		{
			return i;
		}
	}

	return -1;
}

static int process_tcp(int *sock, int *accepted)
{
	static int counter;
	int client;
	int slot;
	struct sockaddr_in6 client_addr;
	socklen_t client_addr_len = sizeof(client_addr);

	client = accept(*sock, (struct sockaddr *)&client_addr,
					&client_addr_len);
	if (client < 0)
	{
		LOG_ERR("Error in accept %d, stopping server", -errno);
		return -errno;
	}

	slot = get_free_slot(accepted);
	if (slot < 0 || slot >= CONFIG_NET_SAMPLE_NUM_HANDLERS)
	{
		LOG_ERR("Cannot accept more connections");
		close(client);
		return 0;
	}

	accepted[slot] = client;

#if defined(CONFIG_NET_IPV6)
	if (client_addr.sin6_family == AF_INET6)
	{
		tcp6_handler_tid[slot] = k_thread_create(
			&tcp6_handler_thread[slot],
			tcp6_handler_stack[slot],
			K_THREAD_STACK_SIZEOF(tcp6_handler_stack[slot]),
			client_conn_handler,
			INT_TO_POINTER(slot),
			&accepted[slot],
			&tcp6_handler_tid[slot],
			THREAD_PRIORITY,
			0, K_NO_WAIT);
	}
#endif

#if defined(CONFIG_NET_IPV4)
	if (client_addr.sin6_family == AF_INET)
	{
		tcp4_handler_tid[slot] = k_thread_create(
			&tcp4_handler_thread[slot],
			tcp4_handler_stack[slot],
			K_THREAD_STACK_SIZEOF(tcp4_handler_stack[slot]),
			client_conn_handler,
			INT_TO_POINTER(slot),
			&accepted[slot],
			&tcp4_handler_tid[slot],
			THREAD_PRIORITY,
			0, K_NO_WAIT);
	}
#endif

	if (LOG_LEVEL >= LOG_LEVEL_DBG)
	{
		char addr_str[INET6_ADDRSTRLEN];

		net_addr_ntop(client_addr.sin6_family,
					  &client_addr.sin6_addr,
					  addr_str, sizeof(addr_str));

		LOG_DBG("[%d] Connection #%d from %s",
				client, ++counter,
				addr_str);
	}

	return 0;
}

static void process_tcp4(void)
{
	struct sockaddr_in addr4;
	int ret;

	(void)memset(&addr4, 0, sizeof(addr4));
	addr4.sin_family = AF_INET;
	addr4.sin_port = htons(MY_PORT);

	ret = setup(&tcp4_listen_sock, (struct sockaddr *)&addr4,
				sizeof(addr4));
	if (ret < 0)
	{
		return;
	}

	LOG_DBG("Waiting for IPv4 HTTP connections on port %d, sock %d",
			MY_PORT, tcp4_listen_sock);

	while (ret == 0 || !want_to_quit)
	{
		ret = process_tcp(&tcp4_listen_sock, tcp4_accepted);
		if (ret < 0)
		{
			return;
		}
	}
}

static void process_tcp6(void)
{
	struct sockaddr_in6 addr6;
	int ret;

	(void)memset(&addr6, 0, sizeof(addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_port = htons(MY_PORT);

	ret = setup(&tcp6_listen_sock, (struct sockaddr *)&addr6,
				sizeof(addr6));
	if (ret < 0)
	{
		return;
	}

	LOG_DBG("Waiting for IPv6 HTTP connections on port %d, sock %d",
			MY_PORT, tcp6_listen_sock);

	while (ret == 0 || !want_to_quit)
	{
		ret = process_tcp(&tcp6_listen_sock, tcp6_accepted);
		if (ret != 0)
		{
			return;
		}
	}
}

void start_listener(void)
{
	int i;

	for (i = 0; i < CONFIG_NET_SAMPLE_NUM_HANDLERS; i++)
	{
#if defined(CONFIG_NET_IPV4)
		tcp4_accepted[i] = -1;
		tcp4_listen_sock = -1;
#endif
#if defined(CONFIG_NET_IPV6)
		tcp6_accepted[i] = -1;
		tcp6_listen_sock = -1;
#endif
	}

	if (IS_ENABLED(CONFIG_NET_IPV6))
	{
		k_thread_start(tcp6_thread_id);
	}

	if (IS_ENABLED(CONFIG_NET_IPV4))
	{
		k_thread_start(tcp4_thread_id);
	}
}
