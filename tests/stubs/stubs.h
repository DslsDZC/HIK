#ifndef HIC_TEST_STUBS_H
#define HIC_TEST_STUBS_H

#include "types.h"

/* hal_stub.c */
void test_hal_set_timestamp(u64 ts);

/* pmm_stub.c */
void test_pmm_reset(void);
u32  test_pmm_alloc_count(void);

/* domain_stub.c */
void test_domain_reset(void);

#endif /* HIC_TEST_STUBS_H */
