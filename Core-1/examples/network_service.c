/*
 * HIK Core-1 Network Service Example
 * 
 * This is an example Core-1 service that provides basic
 * networking functionality.
 */

#include "../include/core1.h"
#include "../include/service.h"
#include "../include/ipc.h"
#include "../include/string.h"

/* Service name and version */
#define SERVICE_NAME "NetworkService"
#define SERVICE_VERSION "1.0.0"

/* Network message types */
#define NET_MSG_SEND 0x01
#define NET_MSG_RECV 0x02
#define NET_MSG_STATUS 0x03

/* Network status */
typedef struct {
    uint32_t connected;
    uint32_t packets_sent;
    uint32_t packets_recv;
    uint32_t bytes_sent;
    uint32_t bytes_recv;
} __attribute__((packed)) net_status_t;

/* Global network status */
static net_status_t g_net_status = {0};

/* Initialize network service */
static int net_service_init(service_context_t *ctx) {
    service_log("Initializing network service...");
    
    /* Initialize network hardware */
    /* In a real implementation, this would initialize the NIC */
    
    service_log("Network service initialized");
    return 0;
}

/* Start network service */
static int net_service_start(service_context_t *ctx) {
    service_log("Starting network service...");
    
    /* Start network hardware */
    /* In a real implementation, this would start the NIC */
    
    g_net_status.connected = 1;
    service_log("Network service started");
    return 0;
}

/* Stop network service */
static int net_service_stop(service_context_t *ctx) {
    service_log("Stopping network service...");
    
    /* Stop network hardware */
    /* In a real implementation, this would stop the NIC */
    
    g_net_status.connected = 0;
    service_log("Network service stopped");
    return 0;
}

/* Cleanup network service */
static int net_service_cleanup(service_context_t *ctx) {
    service_log("Cleaning up network service...");
    
    /* Cleanup network resources */
    /* In a real implementation, this would cleanup the NIC */
    
    service_log("Network service cleaned up");
    return 0;
}

/* Error handler */
static void net_service_error_handler(uint64_t error) {
    service_log_error("Network error occurred", error);
}

/* IPC message handler */
static void net_ipc_handler(ipc_msg_t *msg) {
    if (!msg) {
        return;
    }

    switch (msg->header.data_size) {
        case NET_MSG_SEND: {
            /* Handle send request */
            g_net_status.packets_sent++;
            g_net_status.bytes_sent += msg->header.data_size;
            break;
        }
        case NET_MSG_RECV: {
            /* Handle recv request */
            g_net_status.packets_recv++;
            g_net_status.bytes_recv += msg->header.data_size;
            break;
        }
        case NET_MSG_STATUS: {
            /* Handle status request */
            memcpy(msg->data, &g_net_status, sizeof(net_status_t));
            msg->header.data_size = sizeof(net_status_t);
            msg->header.msg_type = IPC_TYPE_RESPONSE;
            break;
        }
        default:
            service_log_error("Unknown message type", msg->header.data_size);
            break;
    }
}

/* Service main entry point */
void service_main(void) {
    /* Register service callbacks */
    service_callbacks_t callbacks = {
        .init = net_service_init,
        .start = net_service_start,
        .stop = net_service_stop,
        .cleanup = net_service_cleanup,
        .error_handler = net_service_error_handler
    };
    service_register_callbacks(&callbacks);

    /* Initialize service framework */
    service_framework_init(SERVICE_NAME, SERVICE_VERSION, 
                           SERVICE_FLAG_AUTO_START);

    /* Register IPC endpoint */
    ipc_register_endpoint("network", IPC_ENDPOINT_SERVER, net_ipc_handler);

    /* Start service */
    service_start();

    /* Main loop */
    while (1) {
        service_yield();
        service_sleep(10);
        
        /* Process network packets */
        /* In a real implementation, this would process incoming packets */
    }
}