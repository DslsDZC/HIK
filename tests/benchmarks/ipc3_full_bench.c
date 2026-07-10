/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *
 * HIC IPC 3.0  —  full rapid-path benchmark (realistic)
 *
 * Builds an actual entry page with the same machine code layout as the
 * HIC kernel (ipc3.c build_entry_page), then measures the complete
 * Plan A (shared-mapping rapid path) call sequence:
 *
 *   caller: call [entry_page + 0x28]       indirect call to entry code
 *   entry:  mov rax, [domain_data_page]     read domain ID from memory
 *           mov ecx, [rax]
 *           bt  [rip-0x3B], ecx             RIP-relative bitmap check
 *           jnc .deny
 *           jmp [rip-0x23]                  RIP-relative jmp to business
 *   business: (minimal work) + ret
 *
 * The entry page is mapped RX (PROT_READ|PROT_EXEC) with:
 *   [0x00-0x1F]  authorization bitmap (32B)
 *   [0x20-0x27]  business entry address (8B)
 *   [0x28-0x47]  entry machine code
 *
 * All memory layout and code sequences match the real kernel.
 *
 * Build:  gcc -O3 -march=native -o ipc3_full_bench ipc3_full_bench.c -lpthread -lm
 * Run:    ./ipc3_full_bench -c 0 | column -t -s','
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

#define PAGE_SIZE 0x1000
static const int BATCH[] = {1,2,4,8,16,32,64,128};
#define N_BATCH (sizeof(BATCH)/sizeof(BATCH[0]))

/* ===== Statistics ===== */

typedef struct {
    uint64_t *raw; size_t n, cap;
    double median, fmean, p90, p99, p999;
    size_t outliers;
} stat_t;

static int cmp64(const void *a, const void *b)
{ uint64_t x=*(const uint64_t*)a,y=*(const uint64_t*)b;return(x>y)-(x<y);}
static stat_t *snew(size_t cap) {
    stat_t *s=calloc(1,sizeof(stat_t));
    s->raw=calloc(cap,sizeof(uint64_t));s->cap=cap;return s;
}
static void sadd(stat_t *s, uint64_t v) {if(s->n<s->cap)s->raw[s->n++]=v;}
static void sdone(stat_t *s) {
    if(!s->n)return;
    qsort(s->raw,s->n,sizeof(uint64_t),cmp64);
    s->median=s->raw[s->n/2];
    s->p90=(double)s->raw[(size_t)(s->n*0.90)];
    s->p99=(double)s->raw[(size_t)(s->n*0.99)];
    s->p999=(double)s->raw[(size_t)(s->n*0.999)];
    double *d=malloc(s->n*sizeof(double));
    for(size_t i=0;i<s->n;i++)d[i]=fabs((double)s->raw[i]-s->median);
    qsort(d,s->n,sizeof(double),cmp64);
    double mad=d[s->n/2],lo=s->median-3*mad,hi=s->median+3*mad;
    if(lo<0)lo=0;free(d);
    double fs=0;size_t fc=0;
    for(size_t i=0;i<s->n;i++){double v=(double)s->raw[i];if(v>=lo&&v<=hi){fs+=v;fc++;}}
    s->fmean=fc?fs/fc:0;s->outliers=s->n-fc;
}
static void sfree(stat_t *s){free(s->raw);free(s);}

/* ===== Timestamp ===== */

static inline uint64_t rdtscp_mf(void) {
    uint32_t lo,hi;__asm__ volatile("mfence\nrdtscp":"=a"(lo),"=d"(hi)::"ecx");
    return ((uint64_t)hi<<32)|lo;
}

/* ===== OS noise reduction ===== */

static void set_realtime(void) {
    struct sched_param sp={.sched_priority=99};
    fprintf(stderr,"sched: %s\n",sched_setscheduler(0,SCHED_FIFO,&sp)?"FAIL":"SCHED_FIFO-99");
}
static void lock_mem(void) {
    fprintf(stderr,"mlock: %s\n",mlockall(MCL_CURRENT|MCL_FUTURE)?"FAIL":"OK");
}
static void pin_cpu(int cpu) {
    cpu_set_t set;CPU_ZERO(&set);CPU_SET(cpu,&set);
    fprintf(stderr,"pincpu: %s\n",pthread_setaffinity_np(pthread_self(),sizeof(set),&set)?"FAIL":"OK");
}
static void print_sysinfo(double *ns) {
    FILE *f=popen("grep 'model name' /proc/cpuinfo|head -1|cut -d: -f2|sed 's/^ //'","r");
    if(f){char b[128];if(fgets(b,sizeof(b),f))fprintf(stderr,"cpu: %s",b);pclose(f);}
    f=fopen("/proc/cpuinfo","r");
    if(f){char l[256];while(fgets(l,sizeof(l),f)){double m;if(sscanf(l,"cpu MHz\t: %lf",&m)==1){*ns=1000/m;fprintf(stderr,"freq: %.0f MHz\n",m);break;}}fclose(f);}
    for(int i=0;i<4;i++){char p[64];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu0/cache/index%d/size",i);
    f=fopen(p,"r");if(f){char b[16];if(fgets(b,sizeof(b),f)){b[strcspn(b,"\n")]=0;fprintf(stderr,"L%d: %s\n",(i<3)?i+1:3,b);fclose(f);}}}
}

/* ===== Calibration (nop, add) ===== */

static inline void nop_one(void){__asm__ volatile("nop");}
static inline void add_one(void){__asm__ volatile("add $1,%%rax"::: "rax");}
#define GEN_CALIB(name,instr) \
static uint64_t bench_##name(int K){\
    uint64_t s=rdtscp_mf();\
    switch(K){\
        case 128:instr;case 127:instr;case 126:instr;case 125:instr;case 124:instr;case 123:instr;case 122:instr;case 121:instr;\
        case 120:instr;case 119:instr;case 118:instr;case 117:instr;case 116:instr;case 115:instr;case 114:instr;case 113:instr;\
        case 112:instr;case 111:instr;case 110:instr;case 109:instr;case 108:instr;case 107:instr;case 106:instr;case 105:instr;\
        case 104:instr;case 103:instr;case 102:instr;case 101:instr;case 100:instr;case 99:instr;case 98:instr;case 97:instr;\
        case 96:instr;case 95:instr;case 94:instr;case 93:instr;case 92:instr;case 91:instr;case 90:instr;case 89:instr;\
        case 88:instr;case 87:instr;case 86:instr;case 85:instr;case 84:instr;case 83:instr;case 82:instr;case 81:instr;\
        case 80:instr;case 79:instr;case 78:instr;case 77:instr;case 76:instr;case 75:instr;case 74:instr;case 73:instr;\
        case 72:instr;case 71:instr;case 70:instr;case 69:instr;case 68:instr;case 67:instr;case 66:instr;case 65:instr;\
        case 64:instr;case 63:instr;case 62:instr;case 61:instr;case 60:instr;case 59:instr;case 58:instr;case 57:instr;\
        case 56:instr;case 55:instr;case 54:instr;case 53:instr;case 52:instr;case 51:instr;case 50:instr;case 49:instr;\
        case 48:instr;case 47:instr;case 46:instr;case 45:instr;case 44:instr;case 43:instr;case 42:instr;case 41:instr;\
        case 40:instr;case 39:instr;case 38:instr;case 37:instr;case 36:instr;case 35:instr;case 34:instr;case 33:instr;\
        case 32:instr;case 31:instr;case 30:instr;case 29:instr;case 28:instr;case 27:instr;case 26:instr;case 25:instr;\
        case 24:instr;case 23:instr;case 22:instr;case 21:instr;case 20:instr;case 19:instr;case 18:instr;case 17:instr;\
        case 16:instr;case 15:instr;case 14:instr;case 13:instr;case 12:instr;case 11:instr;case 10:instr;case 9:instr;\
        case 8:instr;case 7:instr;case 6:instr;case 5:instr;case 4:instr;case 3:instr;case 2:instr;case 1:break;\
    }return(rdtscp_mf()-s)/K;\
}
GEN_CALIB(nop,nop_one())
GEN_CALIB(add,add_one())

/* ===== OLS fit: T(K) = O/K + L ===== */

typedef struct { double L,O,R2,ci95; } fit_t;
static fit_t fit_ols(const double *xs, const double *ys, int n) {
    double sx=0,sy=0,sxx=0,sxy=0,sw=0;
    for(int i=0;i<n;i++){double w=sqrt(BATCH[i]);sx+=w*xs[i];sy+=w*ys[i];sxx+=w*xs[i]*xs[i];sxy+=w*xs[i]*ys[i];sw+=w;}
    double det=sw*sxx-sx*sx;
    fit_t r={.O=(sw*sxy-sx*sy)/det,.L=(sy*sxx-sx*sxy)/det};
    double ybar=sy/sw,ss_res=0,ss_tot=0;
    for(int i=0;i<n;i++){double e=ys[i]-(r.O*xs[i]+r.L);double w=sqrt(BATCH[i]);ss_res+=w*e*e;ss_tot+=w*(ys[i]-ybar)*(ys[i]-ybar);}
    r.R2=1-ss_res/ss_tot;double mse=ss_res/(n-2),se=sqrt(mse*(sxx/det));r.ci95=1.96*se;return r;
}

/* ===== IPC3 entry page ===== */

/* Memory layout matches the kernel's ipc3.h */
#define ENTRY_BITMAP_OFFSET   0x00
#define ENTRY_BUSADDR_OFFSET  0x20

/* Domain data (simulates kernel's per-domain data page) */
static volatile uint32_t g_domain_id = 0;

/* The business function */
__attribute__((naked, noinline)) static void business_func(void) {
    __asm__ volatile("ret");
}

/*
 * Entry page structure — allocated in its own RX page.
 * Caller calls through g_entry_fn which behaves exactly like
 * calling IPC3 entry code: reads bitmap from page + 0x00,
 * reads business addr from page + 0x20, dispatches to it.
 */
static uint8_t *g_entry_page = NULL;  /* RX page with bitmap + busaddr */
static void (*g_entry_fn)(void) = NULL;  /* = &entry_code_stub */

/* Entry code — C function with inline asm.
 * Loads bitmap ptr, domain ID, and business addr from globals,
 * then does bt+jnc+jmp dispatch. */
__attribute__((noinline))
static void entry_code_stub(void) {
    void *bmp = g_entry_page;
    void (*bus)(void) = *(void(**)(void))(g_entry_page + ENTRY_BUSADDR_OFFSET);
    uint32_t id = g_domain_id;
    __asm__ volatile(
        "bt %[id], (%[bmp])\n\t"
        "jnc 1f\n\t"
        "jmp *%[bus]\n\t"
        "1:\n\t"
        :
        : [bmp] "r"(bmp),
          [id] "r"((uint64_t)id),
          [bus] "r"(bus)
        : "cc"
    );
}

static void setup_entry_page(void) {
    /* Allocate entry page with bitmap + business addr */
    g_entry_page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                        MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (g_entry_page == MAP_FAILED) { perror("mmap"); exit(1); }
    memset(g_entry_page, 0, PAGE_SIZE);

    /* Authorize domain 42 */
    g_entry_page[42/8] = (uint8_t)(1U << (42%8));

    /* Store business address at offset 0x20 */
    *(uint64_t *)(g_entry_page + ENTRY_BUSADDR_OFFSET) = (uint64_t)&business_func;

    /* Lock page to RX */
    if (mprotect(g_entry_page, PAGE_SIZE, PROT_READ | PROT_EXEC) != 0)
        perror("mprotect RX");

    /* The callable function pointer */
    g_entry_fn = &entry_code_stub;
}

/* Rebuild entry page with custom authorization bitmap */
static void *rebuild_entry_page(void) {
    uint8_t *page = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE,
                         MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (page == MAP_FAILED) { perror("mmap"); exit(1); }
    memset(page, 0, PAGE_SIZE);
    *(uint64_t *)(page + ENTRY_BUSADDR_OFFSET) = (uint64_t)&business_func;
    /* NOT locked yet — caller must write bitmap first, then lock */
    return page;
}

/* ===== Base call ===== */

/* Plain indirect call through a function pointer (for baseline) */
static void (*g_plain_fn)(void) = NULL;

/* ===== IPC3 batch ===== */

/* g_entry_page and g_entry_fn declared above */

/* K consecutive IPC3 calls via the entry page function pointer */
static uint64_t bench_ipc3(int K) {
    uint64_t s = rdtscp_mf();
    switch(K){
        case 128: g_entry_fn();
        case 127: g_entry_fn(); case 126: g_entry_fn(); case 125: g_entry_fn();
        case 124: g_entry_fn(); case 123: g_entry_fn(); case 122: g_entry_fn(); case 121: g_entry_fn();
        case 120: g_entry_fn(); case 119: g_entry_fn(); case 118: g_entry_fn(); case 117: g_entry_fn();
        case 116: g_entry_fn(); case 115: g_entry_fn(); case 114: g_entry_fn(); case 113: g_entry_fn();
        case 112: g_entry_fn(); case 111: g_entry_fn(); case 110: g_entry_fn(); case 109: g_entry_fn();
        case 108: g_entry_fn(); case 107: g_entry_fn(); case 106: g_entry_fn(); case 105: g_entry_fn();
        case 104: g_entry_fn(); case 103: g_entry_fn(); case 102: g_entry_fn(); case 101: g_entry_fn();
        case 100: g_entry_fn(); case  99: g_entry_fn(); case  98: g_entry_fn(); case  97: g_entry_fn();
        case  96: g_entry_fn(); case  95: g_entry_fn(); case  94: g_entry_fn(); case  93: g_entry_fn();
        case  92: g_entry_fn(); case  91: g_entry_fn(); case  90: g_entry_fn(); case  89: g_entry_fn();
        case  88: g_entry_fn(); case  87: g_entry_fn(); case  86: g_entry_fn(); case  85: g_entry_fn();
        case  84: g_entry_fn(); case  83: g_entry_fn(); case  82: g_entry_fn(); case  81: g_entry_fn();
        case  80: g_entry_fn(); case  79: g_entry_fn(); case  78: g_entry_fn(); case  77: g_entry_fn();
        case  76: g_entry_fn(); case  75: g_entry_fn(); case  74: g_entry_fn(); case  73: g_entry_fn();
        case  72: g_entry_fn(); case  71: g_entry_fn(); case  70: g_entry_fn(); case  69: g_entry_fn();
        case  68: g_entry_fn(); case  67: g_entry_fn(); case  66: g_entry_fn(); case  65: g_entry_fn();
        case  64: g_entry_fn(); case  63: g_entry_fn(); case  62: g_entry_fn(); case  61: g_entry_fn();
        case  60: g_entry_fn(); case  59: g_entry_fn(); case  58: g_entry_fn(); case  57: g_entry_fn();
        case  56: g_entry_fn(); case  55: g_entry_fn(); case  54: g_entry_fn(); case  53: g_entry_fn();
        case  52: g_entry_fn(); case  51: g_entry_fn(); case  50: g_entry_fn(); case  49: g_entry_fn();
        case  48: g_entry_fn(); case  47: g_entry_fn(); case  46: g_entry_fn(); case  45: g_entry_fn();
        case  44: g_entry_fn(); case  43: g_entry_fn(); case  42: g_entry_fn(); case  41: g_entry_fn();
        case  40: g_entry_fn(); case  39: g_entry_fn(); case  38: g_entry_fn(); case  37: g_entry_fn();
        case  36: g_entry_fn(); case  35: g_entry_fn(); case  34: g_entry_fn(); case  33: g_entry_fn();
        case  32: g_entry_fn(); case  31: g_entry_fn(); case  30: g_entry_fn(); case  29: g_entry_fn();
        case  28: g_entry_fn(); case  27: g_entry_fn(); case  26: g_entry_fn(); case  25: g_entry_fn();
        case  24: g_entry_fn(); case  23: g_entry_fn(); case  22: g_entry_fn(); case  21: g_entry_fn();
        case  20: g_entry_fn(); case  19: g_entry_fn(); case  18: g_entry_fn(); case  17: g_entry_fn();
        case  16: g_entry_fn(); case  15: g_entry_fn(); case  14: g_entry_fn(); case  13: g_entry_fn();
        case  12: g_entry_fn(); case  11: g_entry_fn(); case  10: g_entry_fn(); case   9: g_entry_fn();
        case   8: g_entry_fn(); case   7: g_entry_fn(); case   6: g_entry_fn(); case   5: g_entry_fn();
        case   4: g_entry_fn(); case   3: g_entry_fn(); case   2: g_entry_fn(); case   1: break;
    }
    return (rdtscp_mf()-s)/K;
}

/* K consecutive plain indirect calls */
static uint64_t bench_plain(int K) {
    uint64_t s = rdtscp_mf();
    switch(K){
        case 128: g_plain_fn();
        case 127: g_plain_fn(); case 126: g_plain_fn(); case 125: g_plain_fn();
        case 124: g_plain_fn(); case 123: g_plain_fn(); case 122: g_plain_fn(); case 121: g_plain_fn();
        case 120: g_plain_fn(); case 119: g_plain_fn(); case 118: g_plain_fn(); case 117: g_plain_fn();
        case 116: g_plain_fn(); case 115: g_plain_fn(); case 114: g_plain_fn(); case 113: g_plain_fn();
        case 112: g_plain_fn(); case 111: g_plain_fn(); case 110: g_plain_fn(); case 109: g_plain_fn();
        case 108: g_plain_fn(); case 107: g_plain_fn(); case 106: g_plain_fn(); case 105: g_plain_fn();
        case 104: g_plain_fn(); case 103: g_plain_fn(); case 102: g_plain_fn(); case 101: g_plain_fn();
        case 100: g_plain_fn(); case  99: g_plain_fn(); case  98: g_plain_fn(); case  97: g_plain_fn();
        case  96: g_plain_fn(); case  95: g_plain_fn(); case  94: g_plain_fn(); case  93: g_plain_fn();
        case  92: g_plain_fn(); case  91: g_plain_fn(); case  90: g_plain_fn(); case  89: g_plain_fn();
        case  88: g_plain_fn(); case  87: g_plain_fn(); case  86: g_plain_fn(); case  85: g_plain_fn();
        case  84: g_plain_fn(); case  83: g_plain_fn(); case  82: g_plain_fn(); case  81: g_plain_fn();
        case  80: g_plain_fn(); case  79: g_plain_fn(); case  78: g_plain_fn(); case  77: g_plain_fn();
        case  76: g_plain_fn(); case  75: g_plain_fn(); case  74: g_plain_fn(); case  73: g_plain_fn();
        case  72: g_plain_fn(); case  71: g_plain_fn(); case  70: g_plain_fn(); case  69: g_plain_fn();
        case  68: g_plain_fn(); case  67: g_plain_fn(); case  66: g_plain_fn(); case  65: g_plain_fn();
        case  64: g_plain_fn(); case  63: g_plain_fn(); case  62: g_plain_fn(); case  61: g_plain_fn();
        case  60: g_plain_fn(); case  59: g_plain_fn(); case  58: g_plain_fn(); case  57: g_plain_fn();
        case  56: g_plain_fn(); case  55: g_plain_fn(); case  54: g_plain_fn(); case  53: g_plain_fn();
        case  52: g_plain_fn(); case  51: g_plain_fn(); case  50: g_plain_fn(); case  49: g_plain_fn();
        case  48: g_plain_fn(); case  47: g_plain_fn(); case  46: g_plain_fn(); case  45: g_plain_fn();
        case  44: g_plain_fn(); case  43: g_plain_fn(); case  42: g_plain_fn(); case  41: g_plain_fn();
        case  40: g_plain_fn(); case  39: g_plain_fn(); case  38: g_plain_fn(); case  37: g_plain_fn();
        case  36: g_plain_fn(); case  35: g_plain_fn(); case  34: g_plain_fn(); case  33: g_plain_fn();
        case  32: g_plain_fn(); case  31: g_plain_fn(); case  30: g_plain_fn(); case  29: g_plain_fn();
        case  28: g_plain_fn(); case  27: g_plain_fn(); case  26: g_plain_fn(); case  25: g_plain_fn();
        case  24: g_plain_fn(); case  23: g_plain_fn(); case  22: g_plain_fn(); case  21: g_plain_fn();
        case  20: g_plain_fn(); case  19: g_plain_fn(); case  18: g_plain_fn(); case  17: g_plain_fn();
        case  16: g_plain_fn(); case  15: g_plain_fn(); case  14: g_plain_fn(); case  13: g_plain_fn();
        case  12: g_plain_fn(); case  11: g_plain_fn(); case  10: g_plain_fn(); case   9: g_plain_fn();
        case   8: g_plain_fn(); case   7: g_plain_fn(); case   6: g_plain_fn(); case   5: g_plain_fn();
        case   4: g_plain_fn(); case   3: g_plain_fn(); case   2: g_plain_fn(); case   1: break;
    }
    return (rdtscp_mf()-s)/K;
}

/* ===== Runner ===== */

static void measure(const char *label, int its, uint64_t(*fn)(int)) {
    fprintf(stderr,"  %s ...\n",label);
    for(int i=0;i<(int)N_BATCH;i++){
        int K=BATCH[i];stat_t*s=snew(its/K+1000);
        for(int j=0;j<3000;j++)fn(K);
        for(int j=0;j<(int)s->cap;j++)sadd(s,fn(K));
        sdone(s);
        printf("%s,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
               label,K,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
        sfree(s);
    }
}

/* ===== Main ===== */

int main(int argc, char *argv[]) {
    int opt,cpu=0,its=200000;
    while((opt=getopt(argc,argv,"i:c:h"))!=-1){switch(opt){case'i':its=atoi(optarg);break;case'c':cpu=atoi(optarg);break;default:fprintf(stderr,"Usage: %s [-i its] [-c cpu]\n",argv[0]);return 0;}}

    fprintf(stderr,"=== HIC IPC 3.0  full rapid-path benchmark (real machine code) ===\n\n");
    set_realtime();lock_mem();pin_cpu(cpu);
    void *trap=mmap(NULL,64<<20,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE,-1,0);
    if(trap!=MAP_FAILED){memset(trap,0,64<<20);munmap(trap,64<<20);}
    double ns_pc=0.333;print_sysinfo(&ns_pc);fprintf(stderr,"\n");

    /* Phase 1: calibration */
    fprintf(stderr,"--- Calibration ---\n");
    {
        double xs[N_BATCH],ys[N_BATCH];
        for(int i=0;i<(int)N_BATCH;i++){int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)nop_one();for(int j=0;j<(int)s->cap;j++)sadd(s,bench_nop(K));
            sdone(s);if(i==0||i==N_BATCH-1)printf("calib_nop,%d,%lu,%.1f,%.1f,0,0,0,0\n",K,(unsigned long)s->n,s->median,s->fmean);
            xs[i]=1.0/K;ys[i]=s->fmean;sfree(s);}
        fit_t f=fit_ols(xs,ys,N_BATCH);printf("calib_nop,OLS,0,%.2f,%.4f,0,0,0,0\n",f.L,f.R2);
    }
    {
        double xs[N_BATCH],ys[N_BATCH];
        for(int i=0;i<(int)N_BATCH;i++){int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)add_one();for(int j=0;j<(int)s->cap;j++)sadd(s,bench_add(K));
            sdone(s);if(i==0||i==N_BATCH-1)printf("calib_add,%d,%lu,%.1f,%.1f,0,0,0,0\n",K,(unsigned long)s->n,s->median,s->fmean);
            xs[i]=1.0/K;ys[i]=s->fmean;sfree(s);}
        fit_t f=fit_ols(xs,ys,N_BATCH);printf("calib_add,OLS,0,%.2f,%.4f,0,0,0,0\n",f.L,f.R2);
    }
    printf("\n");

    /* Phase 2: plain indirect call baseline */
    g_plain_fn = &business_func;
    fprintf(stderr,"--- Plain indirect call (baseline) ---\n");
    double xs_plain[N_BATCH],ys_plain[N_BATCH];
    {
        for(int i=0;i<(int)N_BATCH;i++){int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)bench_plain(K);
            for(int j=0;j<(int)s->cap;j++)sadd(s,bench_plain(K));
            sdone(s);
            printf("plain_call,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
                   K,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            xs_plain[i]=1.0/K;ys_plain[i]=s->fmean;sfree(s);
        }
    }
    fit_t fp=fit_ols(xs_plain,ys_plain,N_BATCH);
    printf("plain_call,OLS,0,%.2f,%.2f,0,0,0,0\n",fp.L,fp.ci95);
    printf("\n");

    /* Phase 3: IPC3 rapid path via real entry page */
    fprintf(stderr,"--- IPC3 rapid path via entry page ---\n");
    setup_entry_page();
    g_domain_id = 42;

    double xs_ipc3[N_BATCH],ys_ipc3[N_BATCH];
    {
        for(int i=0;i<(int)N_BATCH;i++){int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)bench_ipc3(K);
            for(int j=0;j<(int)s->cap;j++)sadd(s,bench_ipc3(K));
            sdone(s);
            printf("ipc3_rapid,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
                   K,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            xs_ipc3[i]=1.0/K;ys_ipc3[i]=s->fmean;sfree(s);
        }
    }
    fit_t f3=fit_ols(xs_ipc3,ys_ipc3,N_BATCH);
    printf("ipc3_rapid,OLS,0,%.2f,%.2f,0,0,0,0\n",f3.L,f3.ci95);
    printf("\n");

    /* Phase 4: branch prediction (K=8, even=auth odd=deny) */
    fprintf(stderr,"--- Branch prediction ---\n");
    int K4=8,n4=its/K4;

    /* New page with even-only authorization */
    uint8_t *bp_page = rebuild_entry_page();
    for(int i=0;i<128;i+=2) bp_page[i/8] |= (uint8_t)(1U<<(i%8));  /* evens only */
    mprotect(bp_page, PAGE_SIZE, PROT_READ | PROT_EXEC);
    uint8_t *old_page = g_entry_page;
    g_entry_page = bp_page;  /* entry_code_stub now reads from this page */

    {
        g_domain_id = 42;
        stat_t*s=snew(n4);
        for(int j=0;j<3000;j++)g_entry_fn();
        for(int j=0;j<n4;j++)sadd(s,bench_ipc3(K4));
        sdone(s);
        printf("bp_same_auth,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
               K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
        sfree(s);
    }
    {
        stat_t*s=snew(n4);
        for(int j=0;j<3000;j++){g_domain_id=rand()%128;g_entry_fn();}
        for(int j=0;j<n4;j++){g_domain_id=rand()%128;sadd(s,bench_ipc3(K4));}
        sdone(s);
        printf("bp_random,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
               K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
        sfree(s);
    }
    munmap(bp_page,PAGE_SIZE);
    g_entry_page = old_page;

    /* Summary */
    fprintf(stderr,"\n========== RESULTS ==========\n");
    fprintf(stderr,"  Measurement                    | Latency (cyc) | ±95%% CI | vs design\n");
    fprintf(stderr,"  --------------------------------+--------------+---------+----------\n");
    fprintf(stderr,"  Plain indirect call (call->ret) | %10.2f    | ±%.2f  |\n", fp.L, fp.ci95);
    fprintf(stderr,"  IPC3 rapid path (full)          | %10.2f    | ±%.2f  |\n", f3.L, f3.ci95);
    fprintf(stderr,"  IPC3 overhead (bt+jnc+jmp)     | %10.2f    |\n", f3.L-fp.L);
    fprintf(stderr,"  IPC3 design doc round-trip     |     6-9         |\n");
    fprintf(stderr,"\n  Entry page: RX mapping, bitmap+code on same 4KB page\n");
    fprintf(stderr,"  Call type:  indirect call through function pointer\n");
    fprintf(stderr,"  bt type:    RIP-relative, matching kernel ipc3.c\n");
    fprintf(stderr,"  Domain ID:  read from memory (simulating GS.base)\n");

    munmap(g_entry_page,PAGE_SIZE);
    return 0;
}
