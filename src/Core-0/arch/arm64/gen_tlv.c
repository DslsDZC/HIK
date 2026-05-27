/* 生成 TLV 引导信息二进制块 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BOOT_INFO_MAGIC  0x48494342
#define TAG_MEM_MAP      1
#define TAG_CPU_COUNT    2
#define TAG_KERNEL_BASE  8
#define TAG_KERNEL_SIZE  9
#define TAG_ENTRY_POINT  10
#define TAG_ARCH         17

typedef struct { uint32_t tag, len; } __attribute__((packed)) tlv_t;

typedef struct {
    uint64_t base, length;
    uint32_t type, attr;
} mem_t;

int main() {
    uint8_t buf[4096] = {0};
    uint32_t *hdr = (uint32_t *)buf;
    hdr[0] = BOOT_INFO_MAGIC;  /* magic */
    uint8_t *p = buf + 28;

    uint64_t kbase = 0x40080000, ksize = 0x80000, entry = 0x40080000;
    uint32_t cpu = 4, arch = 2;
    mem_t mem[1] = {{0x40100000, 0x100000, 1, 0}};  /* 1MB usable */

    uint32_t n = 0;
    *(tlv_t *)p = (tlv_t){TAG_MEM_MAP, sizeof(mem)}; p += 8;
    memcpy(p, mem, sizeof(mem)); p += sizeof(mem); n++;

    *(tlv_t *)p = (tlv_t){TAG_CPU_COUNT, 4}; p += 8;
    memcpy(p, &cpu, 4); p += 4; n++;

    *(tlv_t *)p = (tlv_t){TAG_KERNEL_BASE, 8}; p += 8;
    memcpy(p, &kbase, 8); p += 8; n++;

    *(tlv_t *)p = (tlv_t){TAG_KERNEL_SIZE, 8}; p += 8;
    memcpy(p, &ksize, 8); p += 8; n++;

    *(tlv_t *)p = (tlv_t){TAG_ENTRY_POINT, 8}; p += 8;
    memcpy(p, &entry, 8); p += 8; n++;

    *(tlv_t *)p = (tlv_t){TAG_ARCH, 4}; p += 8;
    memcpy(p, &arch, 4); p += 4; n++;

    hdr[1] = (uint32_t)(p - buf);  /* total_size */
    hdr[4] = n;                    /* entry_count */

    fwrite(buf, p - buf, 1, stdout);
    return 0;
}
