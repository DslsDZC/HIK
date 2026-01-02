/*
 * HIK BIOS Bootloader - Security Verification Implementation
 */

#include "verify.h"
#include "sha384.h"
#include "rsa.h"
#include "../hal/hal.h"
#include "../util/util.h"

/* Verification state */
static int verify_enabled = 0;
static int verify_initialized = 0;

/* Public key for signature verification */
static uint8_t public_key[384];  /* RSA-3072 public key */

/*
 * Initialize verification system
 */
int verify_init(void) {
    /* Load public key from fixed location */
    /* For now, use a placeholder key */
    memset(public_key, 0xAA, sizeof(public_key));
    
    verify_initialized = 1;
    
    /* Enable verification by default */
    verify_enabled = 1;
    
    return 1;
}

/*
 * Check if verification is enabled
 */
int verify_is_enabled(void) {
    return verify_enabled;
}

/*
 * Verify kernel signature
 */
int verify_kernel(kernel_header_t *header) {
    uint8_t hash[48];  /* SHA-384 produces 48 bytes */
    uint8_t *signature;
    uint8_t *image_data;
    uint64_t image_size;
    
    if (!verify_initialized) {
        verify_init();
    }
    
    if (!verify_enabled) {
        hal_print("WARNING: Signature verification disabled\n");
        return 1;
    }
    
    hal_print("Verifying kernel signature...\n");

    /* Calculate hash of kernel image */
    image_data = (uint8_t*)KERNEL_LOAD_ADDR;
    /* Calculate total kernel size */
    image_size = header->code_size + header->data_size +
                  header->config_size + header->signature_size;
    
    sha384_context_t ctx;
    sha384_init(&ctx);
    sha384_update(&ctx, image_data, image_size);
    sha384_final(&ctx, hash);
    
    hal_print("Kernel hash: ");
    for (int i = 0; i < 48; i++) {
        hal_print_hex(hash[i]);
        hal_print(" ");
    }
    hal_print("\n");
    
    /* Get signature from kernel image */
    if (header->signature_offset == 0 || header->signature_size == 0) {
        hal_print("ERROR: No signature in kernel image\n");
        return 0;
    }
    
    signature = image_data + header->signature_offset;
    
    /* Verify RSA signature */
    if (!rsa_verify_sha384(hash, signature, public_key)) {
        hal_print("ERROR: Signature verification failed\n");
        return 0;
    }
    
    hal_print("Signature verified successfully\n");
    return 1;
}
