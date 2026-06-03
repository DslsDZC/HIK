#include "unity.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "stubs.h"

#define DOMAIN_SVC  1
#define DOMAIN_NEW  2

static cap_id_t   g_primary_cap;

void setUp(void) {
    test_pmm_reset();
    test_domain_reset();
    test_hal_set_timestamp(4000000);
    capability_system_init();
    cap_init_domain_key(DOMAIN_SVC);
    cap_init_domain_key(DOMAIN_NEW);
}

void tearDown(void) {}

/* ==================== 服务实例能力 ==================== */

void test_create_service_instance_success(void) {
    hic_status_t s = cap_create_service_instance(DOMAIN_SVC,
                                                  "test.svc", 1, 0,
                                                  &g_primary_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, g_primary_cap);
}

void test_create_service_instance_null_name(void) {
    hic_status_t s = cap_create_service_instance(DOMAIN_SVC,
                                                  NULL, 1, 0,
                                                  &g_primary_cap);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, s);
}

/* ==================== 主备切换 ==================== */

void test_service_switch_no_primary_assigned(void) {
    cap_create_service_instance(DOMAIN_SVC, "test.svc", 1, 0, &g_primary_cap);

    cap_id_t standby_cap;
    cap_create_service_instance(DOMAIN_SVC, "test.svc", 2, 0, &standby_cap);

    /* primary_cap 初始为 INVALID，查找服务实例返回 NOT_FOUND */
    cap_id_t old_primary;
    hic_status_t s = cap_service_switch_primary(g_primary_cap, standby_cap,
                                                 &old_primary);
    TEST_ASSERT_EQUAL(HIC_ERROR_NOT_FOUND, s);
}

/* ==================== 连接迁移 ==================== */

void test_connection_migrate_success(void) {
    cap_id_t shmem_cap;
    cap_handle_t handle_from, handle_to;
    hic_status_t s = cap_migration_channel_create(DOMAIN_SVC, DOMAIN_NEW,
                                                   4096,
                                                   &shmem_cap,
                                                   &handle_from,
                                                   &handle_to);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 迁移连接 */
    const char state[] = "test_state_data";
    s = cap_connection_migrate(shmem_cap, DOMAIN_SVC, DOMAIN_NEW,
                               state, sizeof(state));
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 所有者应变更为 DOMAIN_NEW */
    TEST_ASSERT_EQUAL(DOMAIN_NEW, g_global_cap_table[shmem_cap].owner);
}

void test_connection_migrate_wrong_owner(void) {
    cap_id_t shmem_cap;
    cap_handle_t handle_from, handle_to;
    cap_migration_channel_create(DOMAIN_SVC, DOMAIN_NEW, 4096,
                                 &shmem_cap, &handle_from, &handle_to);

    /* DOMAIN_NEW 不能把不属于自己的连接迁移给 SVC */
    hic_status_t s = cap_connection_migrate(shmem_cap, DOMAIN_NEW, DOMAIN_SVC,
                                            NULL, 0);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, s);
}

/* ==================== 运行 ==================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_create_service_instance_success);
    RUN_TEST(test_create_service_instance_null_name);

    RUN_TEST(test_service_switch_no_primary_assigned);

    RUN_TEST(test_connection_migrate_success);
    RUN_TEST(test_connection_migrate_wrong_owner);

    return UNITY_END();
}
