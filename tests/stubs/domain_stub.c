#include "domain.h"
#include <string.h>

/* Minimal domain table for privileged_check_access */
domain_t g_domains[HIC_DOMAIN_MAX];

hic_status_t domain_create(domain_type_t type, domain_id_t parent,
                           const domain_quota_t *quota, domain_id_t *out) {
    (void)type; (void)parent; (void)quota; (void)out;
    return HIC_SUCCESS;
}

hic_status_t domain_destroy(domain_id_t domain_id) { (void)domain_id; return HIC_SUCCESS; }
void domain_system_init(void) {}

bool domain_check_policy_derivation(domain_sched_policy_t parent,
                                     domain_sched_policy_t derived) {
    (void)parent; (void)derived;
    return true;  /* Always allow in tests */
}

bool domain_can_create_sched_policy(domain_id_t domain,
                                     domain_sched_policy_t policy) {
    (void)domain; (void)policy;
    return true;  /* Always allow in tests */
}

domain_sec_level_t domain_get_sec_level(domain_type_t type) {
    (void)type;
    return DOMAIN_SEC_LEVEL_CORE;
}

/* Test helpers */
void test_domain_reset(void) {
    memset(g_domains, 0, sizeof(g_domains));
}
