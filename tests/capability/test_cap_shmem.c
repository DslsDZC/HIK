#include "unity.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "stubs.h"

#define DOMAIN_S  1
#define DOMAIN_T  2

static cap_id_t   g_shmem_cap;
static cap_handle_t g_handle;

void setUp(void) {
    test_pmm_reset();
    test_domain_reset();
    test_hal_set_timestamp(3000000);
    capability_system_init();
    cap_init_domain_key(DOMAIN_S);
    cap_init_domain_key(DOMAIN_T);
}

void tearDown(void) {}

/* ==================== shmem_alloc ==================== */

void test_shmem_alloc_success(void) {
    hic_status_t s = shmem_alloc(DOMAIN_S, 4096, SHMEM_FLAG_WRITABLE,
                                 &g_shmem_cap, &g_handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, g_shmem_cap);
    TEST_ASSERT_NOT_EQUAL(CAP_HANDLE_INVALID, g_handle);

    /* 验证共享内存区域 */
    shmem_region_t info;
    s = shmem_get_info(g_shmem_cap, &info);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_EQUAL(DOMAIN_S, info.owner);
    TEST_ASSERT_EQUAL(1, info.ref_count);
    TEST_ASSERT(info.phys_base != 0);
}

void test_shmem_alloc_zero_size(void) {
    hic_status_t s = shmem_alloc(DOMAIN_S, 0, 0, &g_shmem_cap, &g_handle);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, s);
}

/* ==================== shmem_map / shmem_unmap ==================== */

void test_shmem_map_success(void) {
    cap_handle_t t_handle;

    shmem_alloc(DOMAIN_S, 8192, SHMEM_FLAG_WRITABLE, &g_shmem_cap, &g_handle);

    hic_status_t s = shmem_map(DOMAIN_S, DOMAIN_T, g_shmem_cap,
                               CAP_MEM_READ | CAP_MEM_WRITE, &t_handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_NOT_EQUAL(CAP_HANDLE_INVALID, t_handle);

    /* 引用计数应增加 */
    shmem_region_t info;
    shmem_get_info(g_shmem_cap, &info);
    TEST_ASSERT_EQUAL(2, info.ref_count);

    /* DOMAIN_T 应能访问 */
    s = cap_check_access(DOMAIN_T, t_handle, CAP_MEM_READ | CAP_MEM_WRITE);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 解除映射 */
    s = shmem_unmap(DOMAIN_T, t_handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 引用计数应减少 */
    shmem_get_info(g_shmem_cap, &info);
    TEST_ASSERT_EQUAL(1, info.ref_count);
}

void test_shmem_map_attenuated_rights(void) {
    cap_handle_t t_handle;

    shmem_alloc(DOMAIN_S, 4096, SHMEM_FLAG_WRITABLE, &g_shmem_cap, &g_handle);

    /* 只映射读权限 */
    hic_status_t s = shmem_map(DOMAIN_S, DOMAIN_T, g_shmem_cap,
                               CAP_MEM_READ, &t_handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* T 可以读，不可以写 */
    TEST_ASSERT_EQUAL(HIC_SUCCESS,
        cap_check_access(DOMAIN_T, t_handle, CAP_MEM_READ));
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION,
        cap_check_access(DOMAIN_T, t_handle, CAP_MEM_WRITE));

    shmem_unmap(DOMAIN_T, t_handle);
}

void test_shmem_map_exceeds_rights(void) {
    cap_handle_t t_handle;

    shmem_alloc(DOMAIN_S, 4096, 0, &g_shmem_cap, &g_handle);
    /* shmem_alloc without SHMEM_FLAG_WRITABLE → 只有 CAP_MEM_READ */

    /* 尝试映射写权限应失败 */
    hic_status_t s = shmem_map(DOMAIN_S, DOMAIN_T, g_shmem_cap,
                               CAP_MEM_READ | CAP_MEM_WRITE, &t_handle);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, s);
}

void test_shmem_unmap_releases_memory(void) {
    shmem_alloc(DOMAIN_S, 4096, 0, &g_shmem_cap, &g_handle);

    /* 全部解映射后，能力表项被清零 */
    hic_status_t s = shmem_unmap(DOMAIN_S, g_handle);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* cap_id 被清零，能力不存在 */
    TEST_ASSERT_FALSE(capability_exists(g_shmem_cap));
}

/* ==================== shmem_get_info ==================== */

void test_shmem_get_info_non_shmem_cap(void) {
    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_S, 0x800000, 0x1000, CAP_MEM_READ, &mem_cap);

    shmem_region_t info;
    /* 普通内存能力没有对应的共享内存区域 */
    hic_status_t s = shmem_get_info(mem_cap, &info);
    TEST_ASSERT_EQUAL(HIC_ERROR_NOT_FOUND, s);
}

/* ==================== 运行 ==================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_shmem_alloc_success);
    RUN_TEST(test_shmem_alloc_zero_size);

    RUN_TEST(test_shmem_map_success);
    RUN_TEST(test_shmem_map_attenuated_rights);
    RUN_TEST(test_shmem_map_exceeds_rights);
    RUN_TEST(test_shmem_unmap_releases_memory);

    RUN_TEST(test_shmem_get_info_non_shmem_cap);

    return UNITY_END();
}
