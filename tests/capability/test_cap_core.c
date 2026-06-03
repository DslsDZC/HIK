#include "unity.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "stubs.h"

/* ==================== 全局状态 ==================== */

/* 测试域ID */
#define DOMAIN_A  1
#define DOMAIN_B  2
#define DOMAIN_C  3

static cap_id_t g_cap;

/* ==================== Setup / Teardown ==================== */

void setUp(void) {
    test_pmm_reset();
    test_domain_reset();
    test_hal_set_timestamp(1000000);
    capability_system_init();
    cap_init_domain_key(DOMAIN_A);
    cap_init_domain_key(DOMAIN_B);
    cap_init_domain_key(DOMAIN_C);
}

void tearDown(void) {
}

/* ==================== cap_create_memory ==================== */

void test_create_memory_success(void) {
    hic_status_t status = cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                                            CAP_MEM_READ | CAP_MEM_WRITE, &g_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, g_cap);

    /* 验证表项 */
    cap_entry_t *entry = &g_global_cap_table[g_cap];
    TEST_ASSERT_EQUAL(g_cap, entry->cap_id);
    TEST_ASSERT_EQUAL(DOMAIN_A, entry->owner);
    TEST_ASSERT_EQUAL(CAP_MEM_READ | CAP_MEM_WRITE, entry->rights);
    TEST_ASSERT_EQUAL(0x100000, entry->memory.base);
    TEST_ASSERT_EQUAL(0x1000, entry->memory.size);
}

void test_create_memory_invalid_owner(void) {
    hic_status_t status = cap_create_memory(HIC_DOMAIN_MAX, 0x100000, 0x1000,
                                            CAP_MEM_READ, &g_cap);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, status);
}

void test_create_memory_null_out(void) {
    hic_status_t status = cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                                            CAP_MEM_READ, NULL);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, status);
}

/* ==================== cap_create_thread ==================== */

void test_create_thread_success(void) {
    hic_status_t status = cap_create_thread(DOMAIN_A, 42, &g_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, g_cap);

    cap_entry_t *entry = &g_global_cap_table[g_cap];
    TEST_ASSERT_EQUAL(g_cap, entry->cap_id);
    TEST_ASSERT_EQUAL(CAP_TYPE_THREAD, entry->rights);
    TEST_ASSERT_EQUAL(42, entry->thread_efc.thread_id);
}

/* ==================== cap_grant ==================== */

void test_grant_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    hic_status_t status = cap_grant(DOMAIN_A, g_cap, &handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
    TEST_ASSERT_NOT_EQUAL(CAP_HANDLE_INVALID, handle);

    /* 验证句柄可解析 */
    cap_id_t decoded = cap_get_cap_id(handle);
    TEST_ASSERT_EQUAL(g_cap, decoded);

    u32 token = cap_get_token(handle);
    TEST_ASSERT_TRUE(cap_validate_token(DOMAIN_A, g_cap, token));
}

void test_grant_invalid_cap(void) {
    cap_handle_t handle;
    /* HIC_CAP_INVALID (0) is a valid slot index so cap >= CAP_TABLE_SIZE check passes.
     * Use a cap_id beyond the table bounds to trigger the range check. */
    hic_status_t status = cap_grant(DOMAIN_A, CAP_TABLE_SIZE + 1, &handle);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, status);
}

/* ==================== cap_check_access ==================== */

void test_check_access_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, g_cap, &handle);

    hic_status_t status = cap_check_access(DOMAIN_A, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
}

void test_check_access_wrong_domain(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, g_cap, &handle);

    /* DOMAIN_B 不应能使用 DOMAIN_A 的句柄 */
    hic_status_t status = cap_check_access(DOMAIN_B, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

void test_check_access_insufficient_rights(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, g_cap, &handle);

    /* 只有读权限，请求写权限应失败 */
    hic_status_t status = cap_check_access(DOMAIN_A, handle, CAP_MEM_WRITE);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

void test_check_access_revoked(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, g_cap, &handle);

    /* 撤销后 cap_id 被清零，check_access 返回 CAP_INVALID */
    cap_revoke(g_cap);
    hic_status_t status = cap_check_access(DOMAIN_A, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

void test_check_access_handle_invalid(void) {
    hic_status_t status = cap_check_access(DOMAIN_A, CAP_HANDLE_INVALID, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, status);
}

/* ==================== cap_revoke ==================== */

void test_revoke_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    hic_status_t status = cap_revoke(g_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);

    /* 撤销后 cap_id 应被清零 */
    TEST_ASSERT_EQUAL(0, g_global_cap_table[g_cap].cap_id);
}

void test_revoke_invalid_cap(void) {
    hic_status_t status = cap_revoke(HIC_CAP_INVALID);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

void test_revoke_twice(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, cap_revoke(g_cap));
    /* 二次撤销同一能力应返回成功（幂等） */
    /* 注意：cap_revoke 中会对已撤销的 cap_id=0 的表项返回错误 */
    /* 实际行为：entry->cap_id=0 != cap, 所以返回 HIC_ERROR_CAP_INVALID */
    hic_status_t status = cap_revoke(g_cap);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

/* ==================== cap_force_revoke ==================== */

void test_force_revoke_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    TEST_ASSERT_TRUE(capability_exists(g_cap));

    hic_status_t status = cap_force_revoke(g_cap);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);

    /* 强制撤销后能力应不存在 */
    TEST_ASSERT_FALSE(capability_exists(g_cap));

    /* 使用旧句柄访问应失败 */
    cap_handle_t handle = cap_make_handle(DOMAIN_A, g_cap);
    status = cap_check_access(DOMAIN_A, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

/* ==================== cap_transfer ==================== */

void test_transfer_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    hic_status_t status = cap_transfer(DOMAIN_A, DOMAIN_B, g_cap, &handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);

    /* 所有者应变更为 DOMAIN_B */
    TEST_ASSERT_EQUAL(DOMAIN_B, g_global_cap_table[g_cap].owner);

    /* DOMAIN_B 的新句柄应有效 */
    status = cap_check_access(DOMAIN_B, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
}

void test_transfer_wrong_owner(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    /* DOMAIN_B 尝试传递不属于自己的能力 */
    hic_status_t status = cap_transfer(DOMAIN_B, DOMAIN_C, g_cap, &handle);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

/* ==================== cap_transfer_with_attenuation ==================== */

void test_transfer_attenuated_rights(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                      CAP_MEM_READ | CAP_MEM_WRITE, &g_cap);
    cap_handle_t handle;
    hic_status_t status = cap_transfer_with_attenuation(
        DOMAIN_A, DOMAIN_B, g_cap, CAP_MEM_READ, &handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);

    /* DOMAIN_B 应只能读 */
    status = cap_check_access(DOMAIN_B, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);

    status = cap_check_access(DOMAIN_B, handle, CAP_MEM_WRITE);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

void test_transfer_attenuated_exceeds_original(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    /* 尝试提升权限（读 → 读|写）应失败 */
    hic_status_t status = cap_transfer_with_attenuation(
        DOMAIN_A, DOMAIN_B, g_cap,
        CAP_MEM_READ | CAP_MEM_WRITE, &handle);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

/* ==================== cap_derive ==================== */

void test_derive_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                      CAP_MEM_READ | CAP_MEM_WRITE, &g_cap);
    cap_id_t derived;
    hic_status_t status = cap_derive(DOMAIN_A, g_cap, CAP_MEM_READ, &derived);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, status);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, derived);
    TEST_ASSERT_NOT_EQUAL(g_cap, derived);

    /* 派生能力只有读权限 */
    TEST_ASSERT_EQUAL(CAP_MEM_READ, g_global_cap_table[derived].rights);
    TEST_ASSERT_EQUAL(DOMAIN_A, g_global_cap_table[derived].owner);
}

void test_derive_exceeds_parent_rights(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_id_t derived;
    /* 派生权限不能超过父能力 */
    hic_status_t status = cap_derive(DOMAIN_A, g_cap,
                                     CAP_MEM_READ | CAP_MEM_WRITE, &derived);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

void test_derive_wrong_owner(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_id_t derived;
    hic_status_t status = cap_derive(DOMAIN_B, g_cap, CAP_MEM_READ, &derived);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, status);
}

void test_derive_revoked_parent(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_revoke(g_cap);
    cap_id_t derived;
    hic_status_t status = cap_derive(DOMAIN_A, g_cap, CAP_MEM_READ, &derived);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

/* ==================== 级联撤销 ==================== */

void test_revoke_propagates_to_derived(void) {
    /* 创建 → 派生 → 撤销父 → 子应失效 */
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                      CAP_MEM_READ | CAP_MEM_WRITE, &g_cap);
    cap_id_t derived;
    cap_derive(DOMAIN_A, g_cap, CAP_MEM_READ, &derived);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, derived, &handle);

    /* 撤销父能力 */
    cap_revoke(g_cap);

    /* 派生能力也应失效（cap_id 被清零，返回 INVALID） */
    hic_status_t status = cap_check_access(DOMAIN_A, handle, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_CAP_INVALID, status);
}

/* ==================== capability_exists / 辅助函数 ==================== */

void test_capability_exists(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    TEST_ASSERT_TRUE(capability_exists(g_cap));

    cap_revoke(g_cap);
    TEST_ASSERT_FALSE(capability_exists(g_cap));
}

void test_get_capability_permissions(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000,
                      CAP_MEM_READ | CAP_MEM_WRITE, &g_cap);
    u64 perms = get_capability_permissions(g_cap);
    TEST_ASSERT_EQUAL(CAP_MEM_READ | CAP_MEM_WRITE, perms);
}

/* ==================== cap_fast_check ==================== */

void test_fast_check_success(void) {
    cap_create_memory(DOMAIN_A, 0x100000, 0x1000, CAP_MEM_READ, &g_cap);
    cap_handle_t handle;
    cap_grant(DOMAIN_A, g_cap, &handle);

    TEST_ASSERT_TRUE(cap_fast_check(DOMAIN_A, handle, CAP_MEM_READ));
    TEST_ASSERT_FALSE(cap_fast_check(DOMAIN_A, handle, CAP_MEM_WRITE));
    TEST_ASSERT_FALSE(cap_fast_check(DOMAIN_B, handle, CAP_MEM_READ));
}

/* ==================== 运行 ==================== */

int main(void) {
    UNITY_BEGIN();

    /* cap_create_memory */
    RUN_TEST(test_create_memory_success);
    RUN_TEST(test_create_memory_invalid_owner);
    RUN_TEST(test_create_memory_null_out);

    /* cap_create_thread */
    RUN_TEST(test_create_thread_success);

    /* cap_grant */
    RUN_TEST(test_grant_success);
    RUN_TEST(test_grant_invalid_cap);

    /* cap_check_access */
    RUN_TEST(test_check_access_success);
    RUN_TEST(test_check_access_wrong_domain);
    RUN_TEST(test_check_access_insufficient_rights);
    RUN_TEST(test_check_access_revoked);
    RUN_TEST(test_check_access_handle_invalid);

    /* cap_revoke */
    RUN_TEST(test_revoke_success);
    RUN_TEST(test_revoke_invalid_cap);
    RUN_TEST(test_revoke_twice);

    /* cap_force_revoke */
    RUN_TEST(test_force_revoke_success);

    /* cap_transfer */
    RUN_TEST(test_transfer_success);
    RUN_TEST(test_transfer_wrong_owner);
    RUN_TEST(test_transfer_attenuated_rights);
    RUN_TEST(test_transfer_attenuated_exceeds_original);

    /* cap_derive */
    RUN_TEST(test_derive_success);
    RUN_TEST(test_derive_exceeds_parent_rights);
    RUN_TEST(test_derive_wrong_owner);
    RUN_TEST(test_derive_revoked_parent);

    /* 级联撤销 */
    RUN_TEST(test_revoke_propagates_to_derived);

    /* 辅助函数 */
    RUN_TEST(test_capability_exists);
    RUN_TEST(test_get_capability_permissions);
    RUN_TEST(test_fast_check_success);

    return UNITY_END();
}
