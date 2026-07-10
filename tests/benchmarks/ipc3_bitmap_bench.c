/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *
 * HIC IPC 3.0 位图授权微基准 (v4)
 *
 * bt [bitmap], ecx  — IPC3 核心权限检查指令
 *
 * 入口页 RX 映射 (PROT_READ|PROT_EXEC)，与内核完全一致。
 * bitmap 在页首偏移 0，写入后 mprotect 锁定。
 *
 * 构建: gcc -O3 -march=native -o ipc3_bitmap_bench ipc3_bitmap_bench.c -lpthread -lm
 * 运行: ./ipc3_bitmap_bench
 *       ./ipc3_bitmap_bench -c 1 -i 500000
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <sys/mman.h>
#include <math.h>
#include <getopt.h>

#define PAGE_SIZE  0x1000
#define CACHE_LINE 64

/* 批处理深度 */
static const int BATCH[] = {1, 2, 4, 8, 16, 32, 64};
#define N_BATCH (sizeof(BATCH)/sizeof(BATCH[0]))

/* === 统计 (MAD 自动过滤离群值) === */

typedef struct {
    uint64_t *raw; size_t n, cap;
    double min, max, mean, median, p50, p90, p99, p999, mad, fmean;
    size_t outliers;
} stat_t;

static int cmp64(const void *a, const void *b)
{ uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b; return (x>y)-(x<y); }

static stat_t *snew(size_t cap) {
    stat_t *s = calloc(1,sizeof(stat_t));
    s->raw=calloc(cap,sizeof(uint64_t)); s->cap=cap; s->min=1e100; return s;
}
static void sadd(stat_t *s, uint64_t v) { if (s->n < s->cap) s->raw[s->n++] = v; }
static void sdone(stat_t *s) {
    if (!s->n) return;
    qsort(s->raw,s->n,sizeof(uint64_t),cmp64);
    s->min=s->raw[0]; s->max=s->raw[s->n-1]; s->median=s->raw[s->n/2];
    s->p50=s->median; s->p90=s->raw[(size_t)(s->n*0.90)];
    s->p99=s->raw[(size_t)(s->n*0.99)]; s->p999=s->raw[(size_t)(s->n*0.999)];
    double sum=0; for(size_t i=0;i<s->n;i++) sum+=(double)s->raw[i]; s->mean=sum/s->n;
    double *d=malloc(s->n*sizeof(double));
    for(size_t i=0;i<s->n;i++) d[i]=fabs((double)s->raw[i]-s->median);
    qsort(d,s->n,sizeof(double),cmp64); s->mad=d[s->n/2]; free(d);
    double lo=s->median-3*s->mad, hi=s->median+3*s->mad;
    if(lo<0)lo=0; double fs=0; size_t fc=0;
    for(size_t i=0;i<s->n;i++){double v=(double)s->raw[i];if(v>=lo&&v<=hi){fs+=v;fc++;}}
    s->fmean=fc?fs/fc:0; s->outliers=s->n-fc;
}
static void sfree(stat_t *s) { free(s->raw); free(s); }

/* === RDTSCP === */

static inline uint64_t rdtscp_mf(void) {
    uint32_t lo,hi; __asm__ volatile("mfence\nrdtscp":"=a"(lo),"=d"(hi)::"ecx");
    return ((uint64_t)hi<<32)|lo;
}

/* === OS 降噪 === */

static void set_realtime(void) {
    struct sched_param sp={.sched_priority=99};
    printf("SCHED_FIFO prio=99: %s\n", sched_setscheduler(0,SCHED_FIFO,&sp)
           ? "FAIL (need root)" : "OK");
}
static void lock_mem(void) {
    printf("mlockall: %s\n", mlockall(MCL_CURRENT|MCL_FUTURE) ? "FAIL" : "OK");
}
static void pin_cpu(int cpu) {
    cpu_set_t set; CPU_ZERO(&set); CPU_SET(cpu,&set);
    printf("CPU %d bind: %s\n", cpu, pthread_setaffinity_np(pthread_self(),sizeof(set),&set)
           ? "FAIL" : "OK");
}

/* === 入口页 === */

typedef struct {
    uint8_t  bitmap[32];    /* [0x00] 授权位图 */
    uint64_t business_addr; /* [0x20] 业务页地址 */
    uint8_t  code[0xFD0];   /* [0x28] 入口代码 (无用但占位) */
} __attribute__((packed)) entry_t;

static entry_t *make_entry(void) {
    entry_t *e = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                      MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (e == MAP_FAILED) { perror("mmap"); exit(1); }
    memset(e, 0, PAGE_SIZE);
    return e;
}

static void entry_lock_rx(entry_t *e) {
    if (mprotect(e, PAGE_SIZE, PROT_READ|PROT_EXEC) != 0)
        perror("mprotect RX (non-fatal)");
}

static void entry_auth(entry_t *e, uint32_t domain) {
    e->bitmap[domain/8] |= (uint8_t)(1U << (domain%8));
}

/* === bt 批处理 (Duff's device) === */

static inline void bt_one(uint8_t *bmp, uint32_t id) {
    __asm__ volatile("bt %0,(%1)" : : "r"((uint64_t)id), "r"(bmp) : "cc");
}

#define BT(K) case K: bt_one(bmp, id);
static uint64_t bench_bt(uint8_t *bmp, uint32_t id, int K) {
    uint64_t s = rdtscp_mf();
    switch (K) {
        BT(64) BT(63) BT(62) BT(61) BT(60) BT(59)
        BT(58) BT(57) BT(56) BT(55) BT(54) BT(53)
        BT(52) BT(51) BT(50) BT(49) BT(48) BT(47)
        BT(46) BT(45) BT(44) BT(43) BT(42) BT(41)
        BT(40) BT(39) BT(38) BT(37) BT(36) BT(35)
        BT(34) BT(33) BT(32) BT(31) BT(30) BT(29)
        BT(28) BT(27) BT(26) BT(25) BT(24) BT(23)
        BT(22) BT(21) BT(20) BT(19) BT(18) BT(17)
        BT(16) BT(15) BT(14) BT(13) BT(12) BT(11)
        BT(10) BT(9) BT(8) BT(7) BT(6) BT(5)
        BT(4) BT(3) BT(2) case 1: break;
    }
    return (rdtscp_mf() - s) / K;
}
#undef BT

/* === 运行 === */

static void run_bench(const char *name, uint8_t *bmp, uint32_t id,
                      int K, int its, double ns_pc)
{
    stat_t *s = snew(its);
    for (int i=0; i<2000; i++) bt_one(bmp, id); /* 预热 */
    for (int i=0; i<its; i++) sadd(s, bench_bt(bmp, id, K));
    sdone(s);
    printf("%-42s | %2d | %7lu | %8.1f | %9.1f | %7.1f | %7.1f | %7.0f | %4zu\n",
           name, K, (unsigned long)s->n, s->median*ns_pc,
           s->fmean*ns_pc, s->p90*ns_pc, s->p99*ns_pc, s->p999*ns_pc, s->outliers);
    sfree(s);
}

static void header(void) {
    printf("测试项                                          |  K |  样本 |  P50(ns) | MAD均值 |    P90 |    P99 |   P999 | 离群\n"
           "------------------------------------------------------------------------------\n");
}

/* === CPU 频率 === */

static double cpu_ns(void) {
    FILE *f = fopen("/proc/cpuinfo","r");
    if(f){char l[256];while(fgets(l,sizeof(l),f)){double m;if(sscanf(l,"cpu MHz\t: %lf",&m)==1){fclose(f);return 1000/m;}}fclose(f);}
    return 0.333;
}

/* === MAIN === */

int main(int argc, char *argv[])
{
    int opt, cpu=0, its=200000;
    while((opt=getopt(argc,argv,"i:c:h"))!=-1) {
        switch(opt){
        case'i':its=atoi(optarg);break;
        case'c':cpu=atoi(optarg);break;
        default:printf("Usage: %s [-i its] [-c cpu]\n",argv[0]);return 0;
        }
    }

    printf("HIC IPC 3.0  bt [bitmap], ecx  micro-benchmark  (v4)\n");
    printf("========================================================\n\n");

    set_realtime(); lock_mem(); pin_cpu(cpu);

    /* 陷阱页: 避免首次访问缺页 */
    void *trap = mmap(NULL,64<<20,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE,-1,0);
    if(trap!=MAP_FAILED){memset(trap,0,64<<20);munmap(trap,64<<20);}

    double ns_pc = cpu_ns();
    printf("\nCPU: %.0f MHz  (%.3f ns/cycle)\n\n", 1000/ns_pc, ns_pc);

    /* === 测试 1: 批处理深度 === */
    {
        entry_t *e = make_entry();
        entry_auth(e, 42);
        entry_lock_rx(e);
        uint8_t *bmp = e->bitmap;

        /* 收集原始数据 (周期) 用于拟合 */
        double xs[N_BATCH], ys[N_BATCH], ws[N_BATCH]; /* 1/K, T(cycles), 权重 */
        int nd = 0;

        printf("--- 1. 批处理深度 (domain=42, auth=1, RX page) ---\n");
        header();
        for (int i=0; i<(int)N_BATCH; i++) {
            int K = BATCH[i];
            char lbl[64]; snprintf(lbl,sizeof(lbl),"bt [bitmap],ecx  K=%d",K);
            /* 直接用 run_bench 打印, 同时存 MAD均值(周期) */
            uint8_t *bmp2 = bmp;
            stat_t *s = snew(its/K+1000);
            for (int j=0; j<2000; j++) bt_one(bmp2, 42);
            for (int j=0; j<(int)s->cap; j++) sadd(s, bench_bt(bmp2, 42, K));
            sdone(s);

            double mean_cyc = s->fmean;
            printf("%-42s | %2d | %7lu | %8.1f | %9.1f | %7.1f | %7.1f | %7.0f | %4zu\n",
                   lbl, K, (unsigned long)s->n, s->median*ns_pc,
                   mean_cyc*ns_pc, s->p90*ns_pc, s->p99*ns_pc, s->p999*ns_pc, s->outliers);

            xs[nd] = 1.0 / K; ys[nd] = mean_cyc; ws[nd] = sqrt(K); nd++;
            sfree(s);
        }

        /* 最小二乘法拟合 T(K) = O/K + bt
         * 变换: y = a*x + b, x=1/K, y=T, a=O(overhead), b=bt
         * 加权: 大 K 更稳定, 权 sqrt(K)
         */
        double sx=0, sy=0, sxx=0, sxy=0, sw=0;
        for (int i=0; i<nd; i++) {
            double w = ws[i];
            sx += w * xs[i]; sy += w * ys[i];
            sxx += w * xs[i] * xs[i]; sxy += w * xs[i] * ys[i];
            sw += w;
        }
        double det = sw * sxx - sx * sx;
        double O_fit = (sw * sxy - sx * sy) / det;
        double bt_fit = (sy * sxx - sx * sxy) / det;

        printf("\n  >> 拟合: T(K) = O/K + bt\n");
        printf("  >> O (rdtscp+mfence) = %.1f cycles (%.1f ns)\n", O_fit, O_fit*ns_pc);
        printf("  >> bt [bitmap], ecx   = %.2f cycles (%.2f ns)\n", bt_fit, bt_fit*ns_pc);
        printf("  >> IPC3 设计文档预期: bt = 2-3 周期\n");
        printf("\n");
        munmap(e, PAGE_SIZE);
    }

    /* === 测试 2: 缓存层级 === */
    {
        entry_t *e = make_entry();
        entry_auth(e,42); entry_lock_rx(e);
        uint8_t *bmp = e->bitmap;
        size_t esz = 16<<20;
        uint8_t *ev = malloc(esz); memset(ev,0xAA,esz);

        printf("--- 2. 缓存层级 (K=8) ---\n");
        header();
        int K=8, n=its/K;

        run_bench("L1 Hot", bmp,42,K,n,ns_pc);

        for(size_t i=0;i<32768;i+=64) *(volatile uint64_t*)(ev+i);
        run_bench("L2 (L1 evicted)", bmp,42,K,n,ns_pc);

        for(size_t i=0;i<262144;i+=64) *(volatile uint64_t*)(ev+i);
        run_bench("L3 (L1+L2 evicted)", bmp,42,K,n,ns_pc);

        for(size_t i=0;i<esz;i+=64) *(volatile uint64_t*)(ev+i);
        run_bench("RAM (all evicted)", bmp,42,K,n,ns_pc);

        free(ev); printf("\n");
        munmap(e,PAGE_SIZE);
    }

    /* === 测试 3: 分支预测 === */
    {
        entry_t *e = make_entry();
        for(int i=0;i<128;i++) entry_auth(e,i);
        entry_lock_rx(e);
        uint8_t *bmp = e->bitmap;

        printf("--- 3. 分支预测 (bt+jnc) ---\n");
        header();

        int n=its;
        for(int i=0;i<2000;i++){__asm__ volatile("bt %0,(%1);jnc 99f;99:"::"r"((uint64_t)(i&1)),"r"(bmp):"cc");}

        /* 交替 0/1 */
        {
            stat_t *s=snew(n);
            for(int i=0;i<n;i++){
                uint32_t id=i&1;
                uint64_t t=rdtscp_mf();
                __asm__ volatile("bt %0,(%1);jnc 99f;99:"::"r"((uint64_t)id),"r"(bmp):"cc");
                sadd(s,rdtscp_mf()-t);
            }
            sdone(s);
            printf("%-42s |  1 | %7lu | %8.1f | %9.1f | %7.1f | %7.1f | %7.0f | %4zu\n",
                   "alternate 0/1  (perfect predict)",(unsigned long)s->n,s->median*ns_pc,
                   s->fmean*ns_pc,s->p90*ns_pc,s->p99*ns_pc,s->p999*ns_pc,s->outliers);
            sfree(s);
        }

        /* 随机 0~127 */
        {
            stat_t *s=snew(n);
            for(int i=0;i<n;i++){
                uint32_t id=rand()%128;
                uint64_t t=rdtscp_mf();
                __asm__ volatile("bt %0,(%1);jnc 99f;99:"::"r"((uint64_t)id),"r"(bmp):"cc");
                sadd(s,rdtscp_mf()-t);
            }
            sdone(s);
            printf("%-42s |  1 | %7lu | %8.1f | %9.1f | %7.1f | %7.1f | %7.0f | %4zu\n",
                   "random 0~127  (mis-predict)",(unsigned long)s->n,s->median*ns_pc,
                   s->fmean*ns_pc,s->p90*ns_pc,s->p99*ns_pc,s->p999*ns_pc,s->outliers);
            sfree(s);
        }

        /* 始终 42 */
        {
            stat_t *s=snew(n);
            for(int i=0;i<n;i++){
                uint64_t t=rdtscp_mf();
                __asm__ volatile("bt %0,(%1);jnc 99f;99:"::"r"((uint64_t)42),"r"(bmp):"cc");
                sadd(s,rdtscp_mf()-t);
            }
            sdone(s);
            printf("%-42s |  1 | %7lu | %8.1f | %9.1f | %7.1f | %7.1f | %7.0f | %4zu\n",
                   "always 42  (BTB saturated)",(unsigned long)s->n,s->median*ns_pc,
                   s->fmean*ns_pc,s->p90*ns_pc,s->p99*ns_pc,s->p999*ns_pc,s->outliers);
            sfree(s);
        }

        printf("\n");
        munmap(e,PAGE_SIZE);
    }

    /* === 测试 4: 授权拒绝 === */
    {
        entry_t *e = make_entry();
        entry_auth(e,42);
        entry_lock_rx(e);
        uint8_t *bmp = e->bitmap;

        printf("--- 4. 授权拒绝 (K=8) ---\n");
        header();
        int K=8, n=its/K;
        run_bench("auth granted  (bit=1)  domain=42", bmp,42,K,n,ns_pc);
        run_bench("auth denied   (bit=0)  domain=99", bmp,99,K,n,ns_pc);
        printf("\n");
        munmap(e,PAGE_SIZE);
    }

    /* === 总结 === */
    printf("========================================================\n");
    printf("IPC3 design doc (4 GHz): bt=2-3cyc  total=6-9cyc\n\n");
    printf("Noise reduction:\n");
    printf("  A) SCHED_FIFO + mlockall + CPU pin\n");
    printf("  B) MAD (3x) outlier filter\n");
    printf("  C) K-batch (Duff's device), K bt / K amortize\n");
    printf("  page mapped RX (mprotect PROT_READ|PROT_EXEC)\n");

    return 0;
}
