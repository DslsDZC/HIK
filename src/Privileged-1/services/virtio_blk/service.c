#include "service.h"
#include <stdint.h>

/* External I/O primitives (provided by Core-0) */
extern void serial_print(const char *msg);

/* PCI */
#define PCI_ADDR(bus,dev,fn,off) (0x80000000|(bus<<16)|(dev<<11)|(fn<<8)|(off&0xFC))
#define PCI_VENDOR_ID     0
#define PCI_DEVICE_ID     2
#define PCI_BAR0          0x10
#define PCI_CLASS_REV     0x08

#define VIRTIO_VENDOR     0x1AF4
#define VIRTIO_BLK_LEGACY 0x1001

/* VirtIO Legacy I/O registers (offset from BAR0) */
#define VIRTIO_IO_HOST_FEAT   0
#define VIRTIO_IO_GUEST_FEAT  4
#define VIRTIO_IO_QUEUE_ADDR  8
#define VIRTIO_IO_QUEUE_NUM   12
#define VIRTIO_IO_QUEUE_SEL   14
#define VIRTIO_IO_QUEUE_NOTIFY 16
#define VIRTIO_IO_STATUS      18
#define VIRTIO_IO_ISR         19
#define VIRTIO_IO_DEVICE_FEAT_SEL 20
#define VIRTIO_IO_GUEST_FEAT_SEL  24

/* Device status */
#define VIRTIO_STAT_ACK       1
#define VIRTIO_STAT_DRIVER    2
#define VIRTIO_STAT_DRIVER_OK 4
#define VIRTIO_STAT_FAILED    0x80

/* Feature bits */
#define VIRTIO_BLK_F_BARRIER  0
#define VIRTIO_BLK_F_SIZE_MAX 1
#define VIRTIO_BLK_F_SEG_MAX  2
#define VIRTIO_BLK_F_GEOMETRY 4
#define VIRTIO_BLK_F_RO       5
#define VIRTIO_BLK_F_BLK_SIZE 6
#define VIRTIO_BLK_F_SCSI     7
#define VIRTIO_BLK_F_FLUSH    9
#define VIRTIO_BLK_F_CONFIG_WCE 11

/* Descriptor flags */
#define VRING_DESC_F_NEXT   1
#define VRING_DESC_F_WRITE  2

/* Block request type */
#define VIRTIO_BLK_T_IN     0
#define VIRTIO_BLK_T_OUT    1

/* Status codes */
#define VIRTIO_BLK_S_OK     0

static uint16_t g_io_base = 0;
static uint16_t g_queue_size = 0;
static uint32_t g_queue_pfn = 0;

/* virtq descriptor (16 bytes) */
struct vring_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

/* virtq avail ring */
struct vring_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

/* virtq used ring entry */
struct vring_used_elem {
    uint32_t id;
    uint32_t len;
};

/* virtq used ring */
struct vring_used {
    uint16_t flags;
    uint16_t idx;
    struct vring_used_elem ring[];
};

/* Per-queue memory layout (aligned to PAGE_SIZE) */
struct virtq_mem {
    struct vring_desc desc[256];
    struct vring_avail avail;
    uint16_t avail_ring[256];
    uint8_t pad0[512];
    struct vring_used used;
    struct vring_used_elem used_ring[256];
};

static struct virtq_mem *g_vq = NULL;

static uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "dN"(port));
    return v;
}

static void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "dN"(port));
}
static uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "dN"(port));
    return v;
}
static void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %0, %1" : : "a"(v), "dN"(port));
}

static uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "dN"(port));
    return v;
}

static void outl(uint16_t port, uint32_t v) {
    __asm__ volatile("outl %0, %1" : : "a"(v), "dN"(port));
}

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, PCI_ADDR(bus, dev, fn, off));
    return inl(0xCFC);
}

static int virtio_find_device(void) {
    for (int bus = 0; bus < 256; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read(bus, dev, 0, PCI_VENDOR_ID);
            if ((id & 0xFFFF) == VIRTIO_VENDOR && ((id >> 16) & 0xFFFF) == VIRTIO_BLK_LEGACY) {
                uint32_t bar0 = pci_read(bus, dev, 0, PCI_BAR0);
                g_io_base = bar0 & 0xFFFE;
                serial_print("[VIRTIO] Found VirtIO blk at PCI\n");
                return 0;
            }
        }
    }
    return -1;
}

static int virtio_init(void) {
    if (virtio_find_device() < 0) return -1;

    /* Reset device */
    outb(g_io_base + VIRTIO_IO_STATUS, 0);
    /* Acknowledge + Driver */
    outb(g_io_base + VIRTIO_IO_STATUS, VIRTIO_STAT_ACK | VIRTIO_STAT_DRIVER);

    /* Negotiate features: need VIRTIO_BLK_F_BLK_SIZE for proper sector access */
    outl(g_io_base + VIRTIO_IO_DEVICE_FEAT_SEL, 0);
    uint32_t features = inl(g_io_base + VIRTIO_IO_HOST_FEAT);
    /* Accept all features we support (none special needed) */
    outl(g_io_base + VIRTIO_IO_GUEST_FEAT_SEL, 0);
    outl(g_io_base + VIRTIO_IO_GUEST_FEAT, features & 0);

    /* Setup virtqueue 0 */
    outw(g_io_base + VIRTIO_IO_QUEUE_SEL, 0);
    g_queue_size = inw(g_io_base + VIRTIO_IO_QUEUE_NUM);
    if (g_queue_size > 256) g_queue_size = 256;

    serial_print("[VIRTIO] Queue ready\n");

    /* Allocate physical page for virtqueue (must be physically contiguous, 4K-aligned) */
    /* We use a fixed physical address known to be free */
    g_queue_pfn = 0x1FE000 / 4096;  /* physical frame at 0x1FE000 (~31MB, within identity map) */
    g_vq = (struct virtq_mem *)(uintptr_t)(g_queue_pfn * 4096);
    /* Zero it */
    for (int i = 0; i < 4096; i++) ((uint8_t*)g_vq)[i] = 0;

    outl(g_io_base + VIRTIO_IO_QUEUE_ADDR, g_queue_pfn);

    serial_print("[VIRTIO] Queue PFN assigned\n");

    /* Set DRIVER_OK */
    outb(g_io_base + VIRTIO_IO_STATUS, VIRTIO_STAT_ACK | VIRTIO_STAT_DRIVER | VIRTIO_STAT_DRIVER_OK);

    serial_print("[VIRTIO] Device ready\n");
    return 0;
}

static int virtio_blk_rw(uint32_t lba, void *buffer, int is_write) {
    if (!g_vq || g_queue_size < 3) return -1;

    struct vring_desc *desc = g_vq->desc;
    struct vring_avail *avail = &g_vq->avail;
    volatile struct vring_used *used = &g_vq->used;

    /* Build request header on stack: type(4) + reserved(4) + sector(8) = 16 bytes */
    uint8_t header[16];
    *(uint32_t*)(header + 0) = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    *(uint32_t*)(header + 4) = 0;
    *(uint64_t*)(header + 8) = lba;

    uint8_t status;
    uint16_t idx = avail->idx & 0xFFFF;

    /* Descriptor 0: header (read-only from device perspective) */
    desc[0].addr = (uint64_t)(uintptr_t)header;
    desc[0].len = 16;
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next = 1;

    /* Descriptor 1: data buffer */
    desc[1].addr = (uint64_t)(uintptr_t)buffer;
    desc[1].len = 512;
    desc[1].flags = VRING_DESC_F_NEXT | (is_write ? 0 : VRING_DESC_F_WRITE);
    desc[1].next = 2;

    /* Descriptor 2: status (write-only) */
    desc[2].addr = (uint64_t)(uintptr_t)&status;
    desc[2].len = 1;
    desc[2].flags = VRING_DESC_F_WRITE;
    desc[2].next = 0;

    /* Submit to avail ring */
    avail->ring[idx % g_queue_size] = 0;  /* descriptor head index = 0 */
    __sync_synchronize();
    avail->idx = idx + 1;

    /* Notify device */
    __sync_synchronize();
    outw(g_io_base + VIRTIO_IO_QUEUE_NOTIFY, 0);

    /* Wait for completion (polling) */
    uint32_t last_used = used->idx;
    int timeout = 10000000;
    while (timeout-- > 0) {
        __sync_synchronize();
        if (used->idx != last_used) break;
    }

    if (timeout <= 0) {
        serial_print("[VIRTIO] Timeout\n");
        return -1;
    }

    return (status == VIRTIO_BLK_S_OK) ? 0 : -1;
}

/* ===== Exported interface (same as ide_driver for FAT32 compatibility) ===== */

int ide_read_sector(uint32_t lba, void *buffer) {
    return virtio_blk_rw(lba, buffer, 0);
}

int ide_read_sectors(uint32_t lba, uint8_t count, void *buffer) {
    for (int i = 0; i < count; i++) {
        if (virtio_blk_rw(lba + i, (uint8_t*)buffer + i * 512, 0) < 0)
            return -1;
    }
    return 0;
}

/* ===== Service entry ===== */

int virtio_blk_start(void) {
    serial_print("[VIRTIO] Service started\n");

    if (virtio_init() < 0) {
        serial_print("[VIRTIO] No VirtIO block device found\n");
    } else {
        serial_print("[VIRTIO] VirtIO block device ready\n");
    }

    while (1) {
        for (volatile int i = 0; i < 100000; i++) {}
    }
    return 0;
}
