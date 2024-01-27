#include <zephyr/kernel.h>
#include <errno.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>


#define MY_PORT 8080

#define STACK_SIZE 1024

#if defined(CONFIG_NET_TC_THREAD_COOPERATIVE)
#define THREAD_PRIORITY K_PRIO_COOP(CONFIG_NUM_COOP_PRIORITIES - 1)
#else
#define THREAD_PRIORITY K_PRIO_PREEMPT(8)
#endif

static const char content[] = {
    #include "served_content.html.bin.inc"
};

#define CONFIG_NET_SAMPLE_NUM_HANDLERS 4// TODO: where does it come from?
#define MAX_CLIENT_QUEUE CONFIG_NET_SAMPLE_NUM_HANDLERS

#define EVENT_MASK (NET_EVENT_L4_CONNECTED | \
		    NET_EVENT_L4_DISCONNECTED)

// API
void start_listener(void);
