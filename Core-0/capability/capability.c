/*
 * HIK Core-0 Capability System Implementation
 */

#include "../include/capability.h"
#include "../include/string.h"

/* Global capability system state */
static cap_system_t g_cap_system;

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
 * Initialize capability system
 */
int cap_init(void) {
    memset(&g_cap_system, 0, sizeof(cap_system_t));
    
    g_cap_system.next_cap_handle = 1;
    g_cap_system.next_domain_id = 1;
    g_cap_system.num_caps = 0;
    g_cap_system.num_domains = 0;
    g_cap_system.lock = 0;
    
    return 0;
}

/*
 * Create a new capability
 */
cap_handle_t cap_create(cap_type_t type, uint32_t permissions,
                       uint64_t resource_id, uint64_t base, uint64_t size,
                       uint64_t domain_id) {
    spin_lock(&g_cap_system.lock);
    
    /* Find free capability slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_CAPABILITIES; slot++) {
        if (g_cap_system.capabilities[slot].magic != HIK_CAP_MAGIC) {
            break;
        }
    }
    
    if (slot >= MAX_CAPABILITIES) {
        spin_unlock(&g_cap_system.lock);
        return 0;  /* No free slots */
    }
    
    /* Initialize capability */
    capability_t *cap = &g_cap_system.capabilities[slot];
    cap->magic = HIK_CAP_MAGIC;
    cap->type = type;
    cap->permissions = permissions;
    cap->resource_id = resource_id;
    cap->resource_base = base;
    cap->resource_size = size;
    cap->owner_domain = domain_id;
    cap->ref_count = 1;
    cap->flags = 0;
    
    /* Generate handle */
    cap_handle_t handle = g_cap_system.next_cap_handle++;
    
    /* Add to owner domain */
    domain_t *domain = cap_get_domain(domain_id);
    if (domain) {
        cap_domain_add_cap(domain_id, handle);
    }
    
    g_cap_system.num_caps++;
    
    spin_unlock(&g_cap_system.lock);
    
    return handle;
}

/*
 * Delete a capability
 */
int cap_delete(cap_handle_t handle) {
    spin_lock(&g_cap_system.lock);
    
    capability_t *cap = cap_get_capability(handle);
    if (!cap) {
        spin_unlock(&g_cap_system.lock);
        return -1;
    }
    
    /* Remove from all domains */
    for (uint32_t i = 0; i < MAX_DOMAINS; i++) {
        cap_domain_remove_cap(g_cap_system.domains[i].domain_id, handle);
    }
    
    /* Clear capability */
    memset(cap, 0, sizeof(capability_t));
    
    g_cap_system.num_caps--;
    
    spin_unlock(&g_cap_system.lock);
    
    return 0;
}

/*
 * Grant a capability to a domain
 */
cap_handle_t cap_grant(cap_handle_t handle, uint64_t target_domain_id) {
    spin_lock(&g_cap_system.lock);
    
    capability_t *cap = cap_get_capability(handle);
    if (!cap) {
        spin_unlock(&g_cap_system.lock);
        return 0;
    }
    
    /* Check if grant permission is set */
    if (!(cap->permissions & CAP_PERM_GRANT)) {
        spin_unlock(&g_cap_system.lock);
        return 0;
    }
    
    /* Add to target domain */
    int result = cap_domain_add_cap(target_domain_id, handle);
    if (result == 0) {
        cap->ref_count++;
    }
    
    spin_unlock(&g_cap_system.lock);
    
    return handle;
}

/*
 * Revoke a capability from a domain
 */
int cap_revoke(cap_handle_t handle, uint64_t domain_id) {
    spin_lock(&g_cap_system.lock);
    
    capability_t *cap = cap_get_capability(handle);
    if (!cap) {
        spin_unlock(&g_cap_system.lock);
        return -1;
    }
    
    /* Remove from domain */
    int result = cap_domain_remove_cap(domain_id, handle);
    if (result == 0) {
        cap->ref_count--;
    }
    
    spin_unlock(&g_cap_system.lock);
    
    return result;
}

/*
 * Check if a domain has a capability
 */
int cap_check(uint64_t domain_id, cap_handle_t handle, uint32_t permission) {
    spin_lock(&g_cap_system.lock);
    
    /* Get domain */
    domain_t *domain = cap_get_domain(domain_id);
    if (!domain) {
        spin_unlock(&g_cap_system.lock);
        return -1;
    }
    
    /* Check if domain has the capability */
    int has_cap = 0;
    for (uint32_t i = 0; i < DOMAIN_CAP_SPACE_SIZE; i++) {
        if (domain->cap_space[i] == handle) {
            has_cap = 1;
            break;
        }
    }
    
    if (!has_cap) {
        spin_unlock(&g_cap_system.lock);
        return -2;
    }
    
    /* Check permissions */
    capability_t *cap = cap_get_capability(handle);
    if (!cap || (cap->permissions & permission) != permission) {
        spin_unlock(&g_cap_system.lock);
        return -3;
    }
    
    spin_unlock(&g_cap_system.lock);
    
    return 0;
}

/*
 * Create a new domain
 */
uint64_t cap_create_domain(uint64_t memory_base, uint64_t memory_size) {
    spin_lock(&g_cap_system.lock);
    
    /* Find free domain slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_DOMAINS; slot++) {
        if (g_cap_system.domains[slot].domain_id == 0) {
            break;
        }
    }
    
    if (slot >= MAX_DOMAINS) {
        spin_unlock(&g_cap_system.lock);
        return 0;  /* No free slots */
    }
    
    /* Initialize domain */
    domain_t *domain = &g_cap_system.domains[slot];
    domain->domain_id = g_cap_system.next_domain_id++;
    domain->memory_base = memory_base;
    domain->memory_size = memory_size;
    domain->num_caps = 0;
    domain->state = DOMAIN_STATE_STOPPED;
    
    memset(domain->cap_space, 0, sizeof(domain->cap_space));
    
    g_cap_system.num_domains++;
    
    spin_unlock(&g_cap_system.lock);
    
    return domain->domain_id;
}

/*
 * Delete a domain
 */
int cap_delete_domain(uint64_t domain_id) {
    spin_lock(&g_cap_system.lock);
    
    domain_t *domain = cap_get_domain(domain_id);
    if (!domain) {
        spin_unlock(&g_cap_system.lock);
        return -1;
    }
    
    /* Revoke all capabilities */
    for (uint32_t i = 0; i < DOMAIN_CAP_SPACE_SIZE; i++) {
        if (domain->cap_space[i] != 0) {
            cap_revoke(domain->cap_space[i], domain_id);
        }
    }
    
    /* Clear domain */
    memset(domain, 0, sizeof(domain_t));
    
    g_cap_system.num_domains--;
    
    spin_unlock(&g_cap_system.lock);
    
    return 0;
}

/*
 * Get domain by ID
 */
domain_t* cap_get_domain(uint64_t domain_id) {
    for (uint32_t i = 0; i < MAX_DOMAINS; i++) {
        if (g_cap_system.domains[i].domain_id == domain_id) {
            return &g_cap_system.domains[i];
        }
    }
    return NULL;
}

/*
 * Add capability to domain's capability space
 */
int cap_domain_add_cap(uint64_t domain_id, cap_handle_t handle) {
    domain_t *domain = cap_get_domain(domain_id);
    if (!domain) {
        return -1;
    }
    
    if (domain->num_caps >= DOMAIN_CAP_SPACE_SIZE) {
        return -2;
    }
    
    domain->cap_space[domain->num_caps++] = handle;
    
    return 0;
}

/*
 * Remove capability from domain's capability space
 */
int cap_domain_remove_cap(uint64_t domain_id, cap_handle_t handle) {
    domain_t *domain = cap_get_domain(domain_id);
    if (!domain) {
        return -1;
    }
    
    for (uint32_t i = 0; i < domain->num_caps; i++) {
        if (domain->cap_space[i] == handle) {
            /* Shift remaining capabilities */
            for (uint32_t j = i; j < domain->num_caps - 1; j++) {
                domain->cap_space[j] = domain->cap_space[j + 1];
            }
            domain->cap_space[--domain->num_caps] = 0;
            return 0;
        }
    }
    
    return -2;
}

/*
 * Get capability by handle
 */
capability_t* cap_get_capability(cap_handle_t handle) {
    if (handle == 0 || handle >= g_cap_system.next_cap_handle) {
        return NULL;
    }
    
    /* Search for capability (simplified - in real implementation, use hash table) */
    for (uint32_t i = 0; i < MAX_CAPABILITIES; i++) {
        if (g_cap_system.capabilities[i].magic == HIK_CAP_MAGIC) {
            /* Assume handle maps to index + 1 */
            if (i == (handle - 1) % MAX_CAPABILITIES) {
                return &g_cap_system.capabilities[i];
            }
        }
    }
    
    return NULL;
}

/*
 * Derive a new capability with restricted permissions
 */
cap_handle_t cap_derive(cap_handle_t handle, uint32_t new_permissions) {
    spin_lock(&g_cap_system.lock);
    
    capability_t *orig_cap = cap_get_capability(handle);
    if (!orig_cap) {
        spin_unlock(&g_cap_system.lock);
        return 0;
    }
    
    /* Create new capability with restricted permissions */
    cap_handle_t new_handle = cap_create(
        orig_cap->type,
        orig_cap->permissions & new_permissions,  /* Restrict permissions */
        orig_cap->resource_id,
        orig_cap->resource_base,
        orig_cap->resource_size,
        orig_cap->owner_domain
    );
    
    spin_unlock(&g_cap_system.lock);
    
    return new_handle;
}

/*
 * Dump capability table (for debugging)
 */
void cap_dump(void) {
    /* Would print to console in real implementation */
    /* Simplified for now */
}

/*
 * Dump domain information (for debugging)
 */
void cap_dump_domain(uint64_t domain_id) {
    /* Would print to console in real implementation */
    /* Simplified for now */
}