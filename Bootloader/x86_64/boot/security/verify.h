/*
 * HIK BIOS Bootloader - Security Verification
 */

#ifndef VERIFY_H
#define VERIFY_H

#include <stdint.h>
#include "../stage2/stage2.h"

/* Verify kernel signature */
int verify_kernel(kernel_header_t *header);

/* Initialize verification system */
int verify_init(void);

/* Check if verification is enabled */
int verify_is_enabled(void);

#endif /* VERIFY_H */