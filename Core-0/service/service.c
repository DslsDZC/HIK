/*
 * HIK Core-0 Service Management Implementation
 */

#include "../include/service.h"
#include "../include/mm.h"
#include "../include/capability.h"
#include "../include/sched.h"
#include "../include/string.h"

/* Global service manager state */
static service_manager_t g_service_manager;

/* Spinlock operations */
static inline void spin_lock(uint64_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        /* Spin */
    }
}

static inline void spin_unlock(uint64_t *lock) {
    __sync_lock_release(lock);
}

/*
 * Initialize service manager
 */
int service_init(void) {
    memset(&g_service_manager, 0, sizeof(service_manager_t));
    
    g_service_manager.num_services = 0;
    g_service_manager.next_service_id = 1;
    g_service_manager.lock = 0;
    
    return 0;
}

/*
 * Create a new service
 */
uint64_t service_create(const char *name, uint64_t entry_point,
                       uint64_t code_base, uint64_t code_size,
                       uint64_t data_base, uint64_t data_size) {
    spin_lock(&g_service_manager.lock);
    
    /* Find free service slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_SERVICES; slot++) {
        if (g_service_manager.services[slot].state == 0) {
            break;
        }
    }
    
    if (slot >= MAX_SERVICES) {
        spin_unlock(&g_service_manager.lock);
        return 0;  /* No free slots */
    }
    
    /* Create domain for service */
    uint64_t domain_id = cap_create_domain(code_base, code_size + data_size + STACK_SIZE);
    if (domain_id == 0) {
        spin_unlock(&g_service_manager.lock);
        return 0;
    }
    
    /* Initialize service */
    service_t *service = &g_service_manager.services[slot];
    service->service_id = g_service_manager.next_service_id++;
    strncpy(service->name, name, sizeof(service->name) - 1);
    service->state = SERVICE_STATE_STOPPED;
    service->domain_id = domain_id;
    service->entry_point = entry_point;
    service->code_base = code_base;
    service->code_size = code_size;
    service->data_base = data_base;
    service->data_size = data_size;
    service->stack_base = data_base + data_size;
    service->stack_size = STACK_SIZE;
    service->num_threads = 0;
    service->restart_count = 0;
    service->uptime = 0;
    service->last_error = 0;
    
    /* Create service capability */
    service->cap_handle = cap_create(CAP_TYPE_SERVICE, CAP_PERM_READ | CAP_PERM_WRITE,
                                     service->service_id, 0, 0, domain_id);
    
    g_service_manager.num_services++;
    
    spin_unlock(&g_service_manager.lock);
    
    return service->service_id;
}

/*
 * Start a service
 */
int service_start(uint64_t service_id) {
    spin_lock(&g_service_manager.lock);
    
    service_t *service = service_get(service_id);
    if (!service) {
        spin_unlock(&g_service_manager.lock);
        return -1;
    }
    
    if (service->state != SERVICE_STATE_STOPPED) {
        spin_unlock(&g_service_manager.lock);
        return -2;
    }
    
    /* Create service thread */
    uint64_t thread_id = sched_create_thread(service->domain_id,
                                               (void (*)(void*))service->entry_point,
                                               NULL, THREAD_PRIORITY_NORMAL);
    if (thread_id == 0) {
        spin_unlock(&g_service_manager.lock);
        return -3;
    }
    
    service->state = SERVICE_STATE_RUNNING;
    service->num_threads = 1;
    
    spin_unlock(&g_service_manager.lock);
    
    return 0;
}

/*
 * Stop a service
 */
int service_stop(uint64_t service_id) {
    spin_lock(&g_service_manager.lock);
    
    service_t *service = service_get(service_id);
    if (!service) {
        spin_unlock(&g_service_manager.lock);
        return -1;
    }
    
    if (service->state != SERVICE_STATE_RUNNING) {
        spin_unlock(&g_service_manager.lock);
        return -2;
    }
    
    service->state = SERVICE_STATE_STOPPING;
    
    /* Terminate service threads */
    /* In real implementation, would terminate all threads */
    
    service->state = SERVICE_STATE_STOPPED;
    service->num_threads = 0;
    
    spin_unlock(&g_service_manager.lock);
    
    return 0;
}

/*
 * Restart a service
 */
int service_restart(uint64_t service_id) {
    int result = service_stop(service_id);
    if (result != 0) {
        return result;
    }
    
    service_t *service = service_get(service_id);
    if (service) {
        service->restart_count++;
    }
    
    return service_start(service_id);
}

/*
 * Terminate a service
 */
int service_terminate(uint64_t service_id) {
    spin_lock(&g_service_manager.lock);
    
    service_t *service = service_get(service_id);
    if (!service) {
        spin_unlock(&g_service_manager.lock);
        return -1;
    }
    
    /* Stop service */
    service_stop(service_id);
    
    /* Delete domain */
    cap_delete_domain(service->domain_id);
    
    /* Clear service */
    memset(service, 0, sizeof(service_t));
    
    g_service_manager.num_services--;
    
    spin_unlock(&g_service_manager.lock);
    
    return 0;
}

/*
 * Get service by ID
 */
service_t* service_get(uint64_t service_id) {
    for (uint32_t i = 0; i < MAX_SERVICES; i++) {
        if (g_service_manager.services[i].service_id == service_id) {
            return &g_service_manager.services[i];
        }
    }
    return NULL;
}

/*
 * Get service by name
 */
service_t* service_get_by_name(const char *name) {
    for (uint32_t i = 0; i < MAX_SERVICES; i++) {
        if (strcmp(g_service_manager.services[i].name, name) == 0) {
            return &g_service_manager.services[i];
        }
    }
    return NULL;
}

/*
 * Handle service fault
 */
int service_handle_fault(uint64_t service_id, uint64_t error_code) {
    service_t *service = service_get(service_id);
    if (!service) {
        return -1;
    }
    
    service->state = SERVICE_STATE_ERROR;
    service->last_error = error_code;
    
    /* Attempt restart if restart count is low */
    if (service->restart_count < 3) {
        return service_restart(service_id);
    }
    
    return -2;  /* Restart limit reached */
}

/*
 * Dump service state (for debugging)
 */
void service_dump(uint64_t service_id) {
    service_t *service = service_get(service_id);
    if (!service) {
        return;
    }
    
    /* Would print to console in real implementation */
}

/*
 * Get Core-0 API pointer
 */
core0_api_t* service_get_api(void) {
    static core0_api_t api = {
        .cap_grant = cap_grant,
        .cap_revoke = cap_revoke,
        .cap_check = NULL,  /* Would wrap cap_check */
        .mem_alloc = NULL,  /* Would wrap mm_alloc */
        .mem_free = NULL,   /* Would wrap mm_free */
        .mem_map = NULL,
        .mem_unmap = NULL,
        .ipc_call = NULL,
        .ipc_register = NULL,
        .ipc_unregister = NULL,
        .thread_create = NULL,  /* Would wrap sched_create_thread */
        .thread_exit = NULL,
        .thread_yield = sched_yield,
        .thread_sleep = sched_sleep,
        .inb = NULL,
        .outb = NULL,
        .inw = NULL,
        .outw = NULL,
        .inl = NULL,
        .outl = NULL,
        .log = NULL,
        .log_hex = NULL,
        .service_start = service_start,
        .service_stop = service_stop,
        .service_restart = service_restart
    };
    
    return &api;
}