#include "unity.h"
#include "capability.h"
#include "domain.h"
#include "pmm.h"
#include "hal.h"
#include "stubs.h"

#define DOMAIN_X  1

static cap_id_t g_root_cap;

void setUp(void) {
    test_pmm_reset();
    test_domain_reset();
    test_hal_set_timestamp(2000000);
    capability_system_init();
    cap_init_domain_key(DOMAIN_X);
}

void tearDown(void) {}

/* ==================== cnode_create ==================== */

void test_cnode_create_success(void) {
    hic_status_t s = cnode_create(DOMAIN_X, 6, &g_root_cap);  /* 64 slots */
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, g_root_cap);

    /* 验证 CNode 条目 */
    cap_entry_t *entry = &g_global_cap_table[g_root_cap];
    TEST_ASSERT_EQUAL(g_root_cap, entry->cap_id);
    TEST_ASSERT_TRUE(entry->rights & CAP_CNODE);

    /* 验证 CNode 结构 */
    cnode_t *cnode = (cnode_t*)entry->memory.base;
    TEST_ASSERT_NOT_NULL(cnode);
    TEST_ASSERT_EQUAL(64, cnode->slot_count);
    TEST_ASSERT_EQUAL(6, cnode->slot_bits);
}

void test_cnode_create_invalid_slot_bits(void) {
    /* slot_bits > 12 应失败 */
    hic_status_t s = cnode_create(DOMAIN_X, 13, &g_root_cap);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, s);
}

void test_cnode_create_invalid_owner(void) {
    hic_status_t s = cnode_create(HIC_DOMAIN_MAX, 6, &g_root_cap);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, s);
}

/* ==================== cnode_insert / cnode_remove ==================== */

void test_cnode_insert_memory(void) {
    cnode_create(DOMAIN_X, 6, &g_root_cap);

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x200000, 0x1000, CAP_MEM_READ, &mem_cap);

    hic_status_t s = cnode_insert(g_root_cap, 0, mem_cap, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 验证槽位 */
    cnode_t *cnode = (cnode_t*)g_global_cap_table[g_root_cap].memory.base;
    cnode_slot_t *slot = cnode_get_slots(cnode);
    TEST_ASSERT_EQUAL(mem_cap, slot[0].cap_id);
    TEST_ASSERT_EQUAL(CAP_MEM_READ, slot[0].rights_mask);
    TEST_ASSERT_TRUE(slot[0].flags & CNODE_SLOT_VALID);
}

void test_cnode_insert_exceeds_rights(void) {
    cnode_create(DOMAIN_X, 6, &g_root_cap);

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x200000, 0x1000, CAP_MEM_READ, &mem_cap);

    /* 尝试赋予超过原权限的掩码（写 > 读）应失败 */
    hic_status_t s = cnode_insert(g_root_cap, 0, mem_cap, CAP_MEM_READ | CAP_MEM_WRITE);
    TEST_ASSERT_EQUAL(HIC_ERROR_PERMISSION, s);
}

void test_cnode_insert_out_of_bounds(void) {
    cnode_create(DOMAIN_X, 2, &g_root_cap);  /* 4 slots */

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x200000, 0x1000, CAP_MEM_READ, &mem_cap);

    hic_status_t s = cnode_insert(g_root_cap, 999, mem_cap, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_ERROR_INVALID_PARAM, s);
}

void test_cnode_remove_success(void) {
    cnode_create(DOMAIN_X, 6, &g_root_cap);

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x200000, 0x1000, CAP_MEM_READ, &mem_cap);
    cnode_insert(g_root_cap, 0, mem_cap, CAP_MEM_READ);

    hic_status_t s = cnode_remove(g_root_cap, 0);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    cnode_t *cnode = (cnode_t*)g_global_cap_table[g_root_cap].memory.base;
    cnode_slot_t *slot = cnode_get_slots(cnode);
    TEST_ASSERT_EQUAL(HIC_CAP_INVALID, slot[0].cap_id);
    TEST_ASSERT_EQUAL(CNODE_SLOT_EMPTY, slot[0].flags);
}

/* ==================== cnode_move / cnode_copy ==================== */

void test_cnode_move_success(void) {
    cap_id_t cnode_a, cnode_b;
    cnode_create(DOMAIN_X, 2, &cnode_a);
    cnode_create(DOMAIN_X, 2, &cnode_b);

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x300000, 0x1000, CAP_MEM_READ, &mem_cap);
    cnode_insert(cnode_a, 0, mem_cap, CAP_MEM_READ);

    hic_status_t s = cnode_move(cnode_a, 0, cnode_b, 1);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 源槽位应清空 */
    cnode_t *ca = (cnode_t*)g_global_cap_table[cnode_a].memory.base;
    cnode_slot_t *sa = cnode_get_slots(ca);
    TEST_ASSERT_EQUAL(HIC_CAP_INVALID, sa[0].cap_id);

    /* 目标槽位应有能力 */
    cnode_t *cb = (cnode_t*)g_global_cap_table[cnode_b].memory.base;
    cnode_slot_t *sb = cnode_get_slots(cb);
    TEST_ASSERT_EQUAL(mem_cap, sb[1].cap_id);
}

void test_cnode_copy_success(void) {
    cap_id_t cnode_a, cnode_b;
    cnode_create(DOMAIN_X, 2, &cnode_a);
    cnode_create(DOMAIN_X, 2, &cnode_b);

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x400000, 0x1000, CAP_MEM_READ | CAP_MEM_WRITE, &mem_cap);
    cnode_insert(cnode_a, 0, mem_cap, CAP_MEM_READ | CAP_MEM_WRITE);

    /* 复制并衰减权限 */
    hic_status_t s = cnode_copy(cnode_a, 0, cnode_b, 1, CAP_MEM_READ);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 源槽位不应清空（复制 vs 移动） */
    cnode_t *ca = (cnode_t*)g_global_cap_table[cnode_a].memory.base;
    cnode_slot_t *sa = cnode_get_slots(ca);
    TEST_ASSERT_EQUAL(mem_cap, sa[0].cap_id);

    /* 目标槽位有衰减后的权限 */
    cnode_t *cb = (cnode_t*)g_global_cap_table[cnode_b].memory.base;
    cnode_slot_t *sb = cnode_get_slots(cb);
    TEST_ASSERT_EQUAL(mem_cap, sb[1].cap_id);
    TEST_ASSERT_EQUAL(CAP_MEM_READ, sb[1].rights_mask);
}

/* ==================== cspace_init / cspace_get ==================== */

void test_cspace_init_success(void) {
    hic_status_t s = cspace_init(DOMAIN_X, 6);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    cspace_t *cspace = cspace_get(DOMAIN_X);
    TEST_ASSERT_NOT_NULL(cspace);
    TEST_ASSERT_EQUAL(DOMAIN_X, cspace->owner);
    TEST_ASSERT_NOT_EQUAL(HIC_CAP_INVALID, cspace->root_cnode);
    TEST_ASSERT_TRUE(cspace->flags & CSPACE_FLAG_ACTIVE);
}

void test_cspace_destroy_then_get_null(void) {
    cspace_init(DOMAIN_X, 6);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, cspace_destroy(DOMAIN_X));

    /* 销毁后 cspace_get 应返回 NULL */
    cspace_t *cspace = cspace_get(DOMAIN_X);
    TEST_ASSERT_NULL(cspace);

    /* 二次销毁应安全 */
    TEST_ASSERT_EQUAL(HIC_SUCCESS, cspace_destroy(DOMAIN_X));
}

/* ==================== cptr_lookup ==================== */

void test_cptr_lookup_single_level(void) {
    cspace_init(DOMAIN_X, 6);  /* 64 slots，根 CNode */

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x500000, 0x1000, CAP_MEM_READ, &mem_cap);

    cspace_t *cspace = cspace_get(DOMAIN_X);

    /* 在第 5 个槽位插入能力 */
    cnode_insert(cspace->root_cnode, 5, mem_cap, CAP_MEM_READ);

    /* 用 CPtr 查找：CPtr=5（只有一级，直接指向槽位 5） */
    cap_id_t found_id;
    cap_rights_t found_rights;
    hic_status_t s = cptr_lookup(cspace, (cptr_t)5, &found_id, &found_rights);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);
    TEST_ASSERT_EQUAL(mem_cap, found_id);
    TEST_ASSERT_EQUAL(CAP_MEM_READ, found_rights);
}

/* ==================== cnode_revoke_slot ==================== */

void test_cnode_revoke_slot_success(void) {
    cnode_create(DOMAIN_X, 4, &g_root_cap);  /* 16 slots */

    cap_id_t mem_cap;
    cap_create_memory(DOMAIN_X, 0x600000, 0x1000, CAP_MEM_READ, &mem_cap);
    cnode_insert(g_root_cap, 3, mem_cap, CAP_MEM_READ);

    hic_status_t s = cnode_revoke_slot(g_root_cap, 3);
    TEST_ASSERT_EQUAL(HIC_SUCCESS, s);

    /* 槽位应清空 */
    cnode_t *cnode = (cnode_t*)g_global_cap_table[g_root_cap].memory.base;
    cnode_slot_t *slot = cnode_get_slots(cnode);
    TEST_ASSERT_EQUAL(HIC_CAP_INVALID, slot[3].cap_id);
    TEST_ASSERT_EQUAL(CNODE_SLOT_EMPTY, slot[3].flags);

    /* 能力应被撤销 */
    TEST_ASSERT_FALSE(capability_exists(mem_cap));
}

/* ==================== cnode_tail_scratch ==================== */

void test_cnode_tail_scratch_success(void) {
    cnode_create(DOMAIN_X, 6, &g_root_cap);
    cnode_t *cnode = (cnode_t*)g_global_cap_table[g_root_cap].memory.base;

    void *scratch = cnode_tail_scratch(cnode, 64);
    TEST_ASSERT_NOT_NULL(scratch);

    /* scratch 正好在 CNode 结构 + 槽位数组之后 */
    size_t used = sizeof(cnode_t) + 64 * sizeof(cnode_slot_t);
    TEST_ASSERT((void*)scratch >= (void*)((u8*)cnode + used));
    TEST_ASSERT((u8*)scratch + 1 > (u8*)cnode);  /* 非零地址 */
}

/* ==================== 运行 ==================== */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_cnode_create_success);
    RUN_TEST(test_cnode_create_invalid_slot_bits);
    RUN_TEST(test_cnode_create_invalid_owner);

    RUN_TEST(test_cnode_insert_memory);
    RUN_TEST(test_cnode_insert_exceeds_rights);
    RUN_TEST(test_cnode_insert_out_of_bounds);
    RUN_TEST(test_cnode_remove_success);

    RUN_TEST(test_cnode_move_success);
    RUN_TEST(test_cnode_copy_success);

    RUN_TEST(test_cspace_init_success);
    RUN_TEST(test_cspace_destroy_then_get_null);

    RUN_TEST(test_cptr_lookup_single_level);
    RUN_TEST(test_cnode_revoke_slot_success);
    RUN_TEST(test_cnode_tail_scratch_success);

    return UNITY_END();
}
