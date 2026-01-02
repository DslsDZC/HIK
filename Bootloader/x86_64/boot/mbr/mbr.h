/*
 * HIK BIOS Bootloader - MBR Header
 */

#ifndef MBR_H
#define MBR_H

#include <stdint.h>

/* MBR size */
#define MBR_SIZE 512

/* MBR signature */
#define MBR_SIGNATURE 0xAA55

/* Partition table offset */
#define PARTITION_TABLE_OFFSET 0x1BE

/* Number of partitions */
#define MAX_PARTITIONS 4

/* Partition entry structure */
typedef struct {
    uint8_t  boot_indicator;  /* 0x80 = bootable, 0x00 = non-bootable */
    uint8_t  start_chs[3];    /* Cylinder-Head-Sector start */
    uint8_t  partition_type;  /* Partition type (0x07 = NTFS/exFAT) */
    uint8_t  end_chs[3];      /* Cylinder-Head-Sector end */
    uint32_t lba_start;       /* Starting LBA */
    uint32_t lba_size;        /* Size in sectors */
} __attribute__((packed)) partition_entry_t;

/* Disk Address Packet (DAP) for BIOS INT 13h extensions */
typedef struct {
    uint8_t  packet_size;     /* Size of packet (16 bytes) */
    uint8_t  reserved;        /* Reserved (0) */
    uint16_t buffer_offset;   /* Offset to buffer */
    uint16_t buffer_segment;  /* Segment of buffer */
    uint32_t lba_low;         /* LBA (low 32 bits) */
    uint32_t lba_high;        /* LBA (high 32 bits) */
} __attribute__((packed)) disk_address_packet_t;

/* MBR load address */
#define MBR_LOAD_ADDR 0x7C00

/* MBR relocation address */
#define MBR_RELOC_ADDR 0x6000

/* VBR load address */
#define VBR_LOAD_ADDR 0x7C00

/* Stage 2 load address */
#define STAGE2_LOAD_ADDR 0x10000

/* Common partition types */
#define PARTITION_TYPE_FAT32 0x0C
#define PARTITION_TYPE_NTFS  0x07
#define PARTITION_TYPE_LINUX 0x83
#define PARTITION_TYPE_EXFAT 0x07

#endif /* MBR_H */