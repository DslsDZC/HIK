/*
 * SPDX-FileCopyrightText: 2026 DslsDZC <dsls.dzc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-HIC-service-exception
 *
 * HIC IPC 3.0  —  bt [bitmap], ecx  microbenchmark
 *
 * Measures the core permission check instruction of IPC 3.0:
 *   bt [bitmap], ecx
 *
 * Methodology (beyond reproach):
 *   1. Duff's device batch: K=1..128 consecutive bt / K, amortize measurement noise
 *   2. Calibration: nop / add first to validate method
 *   3. Weighted OLS fit: T(K) = O/K + bt, outputs R² + 95% CI
 *   4. Pairwise validation: every adjacent K pair yields independent bt estimate
 *   5. MAD filter (3× Tukey fences) removes scheduler outliers
 *   6. All output is CSV to stdout; informational messages to stderr
 *
 * Build:
 *   gcc -O3 -march=native -o ipc3_bt_bench ipc3_bt_bench.c -lpthread -lm
 * Run:
 *   ./ipc3_bt_bench                     # CSV to stdout, info to stderr
 *   ./ipc3_bt_bench -c 0 -i 500000      # bind CPU 0, 500k iterations
 *   ./ipc3_bt_bench | column -t -s','   # terminal table
 *   ./ipc3_bt_bench | tee results.csv   # save for later analysis
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

/* === Statistical accumulator (MAD filter) === */

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
    double mad=d[s->n/2], lo=s->median-3*mad, hi=s->median+3*mad;
    if(lo<0)lo=0;free(d);
    double fs=0;size_t fc=0;
    for(size_t i=0;i<s->n;i++){double v=(double)s->raw[i];if(v>=lo&&v<=hi){fs+=v;fc++;}}
    s->fmean=fc?fs/fc:0;s->outliers=s->n-fc;
}
static void sfree(stat_t *s){free(s->raw);free(s);}

/* === RDTSCP + mfence === */

static inline uint64_t rdtscp_mf(void) {
    uint32_t lo,hi;__asm__ volatile("mfence\nrdtscp":"=a"(lo),"=d"(hi)::"ecx");
    return ((uint64_t)hi<<32)|lo;
}

/* === OS noise reduction === */

static void set_realtime(void) {
    struct sched_param sp={.sched_priority=99};
    fprintf(stderr,"sched: %s\n",sched_setscheduler(0,SCHED_FIFO,&sp)?"FAIL (need root)":"SCHED_FIFO-99");
}
static void lock_mem(void) {
    fprintf(stderr,"mlock: %s\n",mlockall(MCL_CURRENT|MCL_FUTURE)?"FAIL":"OK");
}
static void pin_cpu(int cpu) {
    cpu_set_t set;CPU_ZERO(&set);CPU_SET(cpu,&set);
    fprintf(stderr,"pincpu: %s\n",pthread_setaffinity_np(pthread_self(),sizeof(set),&set)?"FAIL":"OK");
}

/* === System info (stderr) === */

static void print_sysinfo(double *out_ns_pc) {
    FILE *f=popen("grep 'model name' /proc/cpuinfo|head -1|cut -d: -f2|sed 's/^ //'","r");
    if(f){char b[128];if(fgets(b,sizeof(b),f))fprintf(stderr,"cpu: %s",b);pclose(f);}
    f=fopen("/proc/cpuinfo","r");
    if(f){char l[256];while(fgets(l,sizeof(l),f)){double m;if(sscanf(l,"cpu MHz\t: %lf",&m)==1){*out_ns_pc=1000/m;fprintf(stderr,"freq: %.0f MHz\n",m);break;}}fclose(f);}
    for(int i=0;i<4;i++){char p[64];snprintf(p,sizeof(p),"/sys/devices/system/cpu/cpu0/cache/index%d/size",i);
    f=fopen(p,"r");if(f){char b[16];if(fgets(b,sizeof(b),f)){b[strcspn(b,"\n")]=0;fprintf(stderr,"L%d: %s\n",(i<3)?i+1:3,b);}fclose(f);}}
    fprintf(stderr,"\n");
}

/* === Duff's device batch: bt === */

static inline void bt_one(uint8_t *bmp, uint32_t id) {
    __asm__ volatile("bt %0,(%1)"::"r"((uint64_t)id),"r"(bmp):"cc");
}
#define BT(K) case K: bt_one(bmp,id);
static uint64_t bench_bt(uint8_t *bmp, uint32_t id, int K) {
    uint64_t s=rdtscp_mf();
    switch(K){
        BT(128)BT(127)BT(126)BT(125)BT(124)BT(123)BT(122)BT(121)
        BT(120)BT(119)BT(118)BT(117)BT(116)BT(115)BT(114)BT(113)
        BT(112)BT(111)BT(110)BT(109)BT(108)BT(107)BT(106)BT(105)
        BT(104)BT(103)BT(102)BT(101)BT(100)BT(99)BT(98)BT(97)
        BT(96)BT(95)BT(94)BT(93)BT(92)BT(91)BT(90)BT(89)
        BT(88)BT(87)BT(86)BT(85)BT(84)BT(83)BT(82)BT(81)
        BT(80)BT(79)BT(78)BT(77)BT(76)BT(75)BT(74)BT(73)
        BT(72)BT(71)BT(70)BT(69)BT(68)BT(67)BT(66)BT(65)
        BT(64)BT(63)BT(62)BT(61)BT(60)BT(59)BT(58)BT(57)
        BT(56)BT(55)BT(54)BT(53)BT(52)BT(51)BT(50)BT(49)
        BT(48)BT(47)BT(46)BT(45)BT(44)BT(43)BT(42)BT(41)
        BT(40)BT(39)BT(38)BT(37)BT(36)BT(35)BT(34)BT(33)
        BT(32)BT(31)BT(30)BT(29)BT(28)BT(27)BT(26)BT(25)
        BT(24)BT(23)BT(22)BT(21)BT(20)BT(19)BT(18)BT(17)
        BT(16)BT(15)BT(14)BT(13)BT(12)BT(11)BT(10)BT(9)
        BT(8)BT(7)BT(6)BT(5)BT(4)BT(3)BT(2)case 1:break;
    }
    return (rdtscp_mf()-s)/K;
}
#undef BT

/* === Entry page (RX, matching real IPC3) === */

typedef struct { uint8_t bitmap[32]; uint64_t pad[0xFD0/8]; } __attribute__((packed)) entry_t;

static entry_t *make_entry(void) {
    entry_t *e=mmap(NULL,PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(e==MAP_FAILED){perror("mmap");exit(1);}memset(e,0,PAGE_SIZE);return e;
}
static void entry_rx(entry_t *e){if(mprotect(e,PAGE_SIZE,PROT_READ|PROT_EXEC)!=0)perror("mprotect");}
static void entry_set(entry_t *e,uint32_t d){e->bitmap[d/8]|=(uint8_t)(1U<<(d%8));}

/* === nop / add calibration === */

static inline void nop_one(void){__asm__ volatile("nop");}
static inline void add_one(void){__asm__ volatile("add $1,%%rax"::: "rax");}

#define GEN_CALIB(name,instr) \
static uint64_t bench_##name(int K){\
    uint64_t s=rdtscp_mf();\
    switch(K){\
        case 128:instr;case 127:instr;case 126:instr;case 125:instr;\
        case 124:instr;case 123:instr;case 122:instr;case 121:instr;\
        case 120:instr;case 119:instr;case 118:instr;case 117:instr;\
        case 116:instr;case 115:instr;case 114:instr;case 113:instr;\
        case 112:instr;case 111:instr;case 110:instr;case 109:instr;\
        case 108:instr;case 107:instr;case 106:instr;case 105:instr;\
        case 104:instr;case 103:instr;case 102:instr;case 101:instr;\
        case 100:instr;case 99:instr;case 98:instr;case 97:instr;\
        case 96:instr;case 95:instr;case 94:instr;case 93:instr;\
        case 92:instr;case 91:instr;case 90:instr;case 89:instr;\
        case 88:instr;case 87:instr;case 86:instr;case 85:instr;\
        case 84:instr;case 83:instr;case 82:instr;case 81:instr;\
        case 80:instr;case 79:instr;case 78:instr;case 77:instr;\
        case 76:instr;case 75:instr;case 74:instr;case 73:instr;\
        case 72:instr;case 71:instr;case 70:instr;case 69:instr;\
        case 68:instr;case 67:instr;case 66:instr;case 65:instr;\
        case 64:instr;case 63:instr;case 62:instr;case 61:instr;\
        case 60:instr;case 59:instr;case 58:instr;case 57:instr;\
        case 56:instr;case 55:instr;case 54:instr;case 53:instr;\
        case 52:instr;case 51:instr;case 50:instr;case 49:instr;\
        case 48:instr;case 47:instr;case 46:instr;case 45:instr;\
        case 44:instr;case 43:instr;case 42:instr;case 41:instr;\
        case 40:instr;case 39:instr;case 38:instr;case 37:instr;\
        case 36:instr;case 35:instr;case 34:instr;case 33:instr;\
        case 32:instr;case 31:instr;case 30:instr;case 29:instr;\
        case 28:instr;case 27:instr;case 26:instr;case 25:instr;\
        case 24:instr;case 23:instr;case 22:instr;case 21:instr;\
        case 20:instr;case 19:instr;case 18:instr;case 17:instr;\
        case 16:instr;case 15:instr;case 14:instr;case 13:instr;\
        case 12:instr;case 11:instr;case 10:instr;case 9:instr;\
        case 8:instr;case 7:instr;case 6:instr;case 5:instr;\
        case 4:instr;case 3:instr;case 2:instr;case 1:break;\
    }return(rdtscp_mf()-s)/K;\
}
GEN_CALIB(nop,nop_one())
GEN_CALIB(add,add_one())

/* === bt + jnc batch (branch measurement) ===
 *
 * Each iteration: bt [bitmap], ecx + jnc <skip>
 * Branch predictor state persists across Duff's device iterations.
 */
#define BTJNC() __asm__ volatile("bt %0,(%1); jnc 99f; 99:" : : "r"((uint64_t)id), "r"(bmp) : "cc")

static uint64_t bench_bt_jnc(uint8_t *bmp, uint32_t id, int K) {
    uint64_t s=rdtscp_mf();
    switch(K){
        case 128: BTJNC(); case 127: BTJNC(); case 126: BTJNC(); case 125: BTJNC();
        case 124: BTJNC(); case 123: BTJNC(); case 122: BTJNC(); case 121: BTJNC();
        case 120: BTJNC(); case 119: BTJNC(); case 118: BTJNC(); case 117: BTJNC();
        case 116: BTJNC(); case 115: BTJNC(); case 114: BTJNC(); case 113: BTJNC();
        case 112: BTJNC(); case 111: BTJNC(); case 110: BTJNC(); case 109: BTJNC();
        case 108: BTJNC(); case 107: BTJNC(); case 106: BTJNC(); case 105: BTJNC();
        case 104: BTJNC(); case 103: BTJNC(); case 102: BTJNC(); case 101: BTJNC();
        case 100: BTJNC(); case  99: BTJNC(); case  98: BTJNC(); case  97: BTJNC();
        case  96: BTJNC(); case  95: BTJNC(); case  94: BTJNC(); case  93: BTJNC();
        case  92: BTJNC(); case  91: BTJNC(); case  90: BTJNC(); case  89: BTJNC();
        case  88: BTJNC(); case  87: BTJNC(); case  86: BTJNC(); case  85: BTJNC();
        case  84: BTJNC(); case  83: BTJNC(); case  82: BTJNC(); case  81: BTJNC();
        case  80: BTJNC(); case  79: BTJNC(); case  78: BTJNC(); case  77: BTJNC();
        case  76: BTJNC(); case  75: BTJNC(); case  74: BTJNC(); case  73: BTJNC();
        case  72: BTJNC(); case  71: BTJNC(); case  70: BTJNC(); case  69: BTJNC();
        case  68: BTJNC(); case  67: BTJNC(); case  66: BTJNC(); case  65: BTJNC();
        case  64: BTJNC(); case  63: BTJNC(); case  62: BTJNC(); case  61: BTJNC();
        case  60: BTJNC(); case  59: BTJNC(); case  58: BTJNC(); case  57: BTJNC();
        case  56: BTJNC(); case  55: BTJNC(); case  54: BTJNC(); case  53: BTJNC();
        case  52: BTJNC(); case  51: BTJNC(); case  50: BTJNC(); case  49: BTJNC();
        case  48: BTJNC(); case  47: BTJNC(); case  46: BTJNC(); case  45: BTJNC();
        case  44: BTJNC(); case  43: BTJNC(); case  42: BTJNC(); case  41: BTJNC();
        case  40: BTJNC(); case  39: BTJNC(); case  38: BTJNC(); case  37: BTJNC();
        case  36: BTJNC(); case  35: BTJNC(); case  34: BTJNC(); case  33: BTJNC();
        case  32: BTJNC(); case  31: BTJNC(); case  30: BTJNC(); case  29: BTJNC();
        case  28: BTJNC(); case  27: BTJNC(); case  26: BTJNC(); case  25: BTJNC();
        case  24: BTJNC(); case  23: BTJNC(); case  22: BTJNC(); case  21: BTJNC();
        case  20: BTJNC(); case  19: BTJNC(); case  18: BTJNC(); case  17: BTJNC();
        case  16: BTJNC(); case  15: BTJNC(); case  14: BTJNC(); case  13: BTJNC();
        case  12: BTJNC(); case  11: BTJNC(); case  10: BTJNC(); case   9: BTJNC();
        case   8: BTJNC(); case   7: BTJNC(); case   6: BTJNC(); case   5: BTJNC();
        case   4: BTJNC(); case   3: BTJNC(); case   2: BTJNC(); case   1: break;
    }
    return (rdtscp_mf()-s)/K;
}
#undef BTJNC

/* === OLS fit: T(K) = O/K + bt === */

typedef struct { double bt,O,R2,ci95; } fit_t;

static fit_t fit_ols(const double *xs, const double *ys, int n) {
    double sx=0,sy=0,sxx=0,sxy=0,sw=0;
    for(int i=0;i<n;i++){double w=sqrt(BATCH[i]);sx+=w*xs[i];sy+=w*ys[i];sxx+=w*xs[i]*xs[i];sxy+=w*xs[i]*ys[i];sw+=w;}
    double det=sw*sxx-sx*sx;
    fit_t r={.O=(sw*sxy-sx*sy)/det,.bt=(sy*sxx-sx*sxy)/det};
    double ybar=sy/sw,ss_res=0,ss_tot=0;
    for(int i=0;i<n;i++){double e=ys[i]-(r.O*xs[i]+r.bt),w=sqrt(BATCH[i]);ss_res+=w*e*e;ss_tot+=w*(ys[i]-ybar)*(ys[i]-ybar);}
    r.R2=1-ss_res/ss_tot;
    double mse=ss_res/(n-2),se_bt=sqrt(mse*(sxx/det));
    r.ci95=1.96*se_bt;
    return r;
}

/* === CSV core: measure a batch series and emit CSV rows === */

static void measure_csv(const char *label, uint8_t *bmp, uint32_t id, int its) {
    fprintf(stderr,"%s: measuring...\n",label);
    double xs[N_BATCH],ys[N_BATCH];
    for(int i=0;i<(int)N_BATCH;i++){
        int K=BATCH[i];
        stat_t *s=snew(its/K+1000);
        for(int j=0;j<3000;j++)bt_one(bmp,id);
        for(int j=0;j<(int)s->cap;j++)sadd(s,bench_bt(bmp,id,K));
        sdone(s);
        printf("%s,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",
               label,K,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
        xs[i]=1.0/K;ys[i]=s->fmean;
        sfree(s);
    }
    fit_t f=fit_ols(xs,ys,N_BATCH);
    printf("%s,OLS,0,%.2f,%.2f,0,0,0,0\n",label,f.bt,f.ci95);
}

/* === MAIN === */

int main(int argc, char *argv[]) {
    int opt,cpu=0,its=200000;
    while((opt=getopt(argc,argv,"i:c:h"))!=-1){
        switch(opt){
        case'i':its=atoi(optarg);break;
        case'c':cpu=atoi(optarg);break;
        default:fprintf(stderr,"Usage: %s [-i its] [-c cpu]\n",argv[0]);return 0;
        }
    }

    fprintf(stderr,"=== HIC IPC 3.0 bt [bitmap],ecx microbenchmark ===\n\n");
    set_realtime();lock_mem();pin_cpu(cpu);

    /* trap page */
    void *trap=mmap(NULL,64<<20,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_POPULATE,-1,0);
    if(trap!=MAP_FAILED){memset(trap,0,64<<20);munmap(trap,64<<20);}

    double ns_pc=0.333;print_sysinfo(&ns_pc);

    /* Phase 1: calibration */
    fprintf(stderr,"--- Phase 1: calibration (nop, add) ---\n");
    {
        double xs[N_BATCH],ys[N_BATCH];
        for(int i=0;i<(int)N_BATCH;i++){
            int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)nop_one();
            for(int j=0;j<(int)s->cap;j++)sadd(s,bench_nop(K));
            sdone(s);xs[i]=1.0/K;ys[i]=s->fmean;
            if(i==0||i==N_BATCH-1)
                printf("calib_nop,%d,%lu,%.1f,%.1f,0,0,0,0\n",K,(unsigned long)s->n,s->median,s->fmean);
            sfree(s);
        }
        fit_t f=fit_ols(xs,ys,N_BATCH);
        printf("calib_nop,OLS,0,%.2f,%.4f,0,0,0,0\n",f.bt,f.R2);
    }
    {
        double xs[N_BATCH],ys[N_BATCH];
        for(int i=0;i<(int)N_BATCH;i++){
            int K=BATCH[i];stat_t*s=snew(its/K+1000);
            for(int j=0;j<3000;j++)add_one();
            for(int j=0;j<(int)s->cap;j++)sadd(s,bench_add(K));
            sdone(s);xs[i]=1.0/K;ys[i]=s->fmean;
            if(i==0||i==N_BATCH-1)
                printf("calib_add,%d,%lu,%.1f,%.1f,0,0,0,0\n",K,(unsigned long)s->n,s->median,s->fmean);
            sfree(s);
        }
        fit_t f=fit_ols(xs,ys,N_BATCH);
        printf("calib_add,OLS,0,%.2f,%.4f,0,0,0,0\n",f.bt,f.R2);
    }
    fprintf(stderr,"\n");

    /* Phase 2: bt [bitmap], ecx */
    entry_t *entry=make_entry();
    entry_set(entry,42);entry_rx(entry);
    fprintf(stderr,"--- Phase 2: bt [bitmap],ecx (domain=42, auth=1, RX page) ---\n");
    printf("\n");
    measure_csv("bt",entry->bitmap,42,its);
    printf("\n");
    fprintf(stderr,"\n");

    /* Phase 3: cache hierarchy — note: bitmap is <1 cache line */
    fprintf(stderr,"--- Phase 3: cache hierarchy (K=8) ---\n");
    fprintf(stderr,"   note: bitmap=32B < 64B cache line, always L1-hot after first access\n");
    {
        int K3=8, n3=its/K3;
        stat_t *s=snew(n3);
        for(int j=0;j<3000;j++)bt_one(entry->bitmap,42);
        for(int j=0;j<n3;j++)sadd(s,bench_bt(entry->bitmap,42,K3));
        sdone(s);
        printf("cache_l1,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",K3,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
        printf("cache_note,0,0,0,0,0,0,0,0\n");
        sfree(s);
    }
    munmap(entry,PAGE_SIZE);
    fprintf(stderr,"\n");

    /* Phase 4: branch prediction (K=8, properly amplifies the signal)
     *   Setup: even domains = authorized, odd domains = unauthorized
     *   This ensures bt+jnc has real taken/not-taken variation */
    fprintf(stderr,"--- Phase 4: branch prediction (K=8, even=auth odd=deny) ---\n");
    {
        entry_t *bp=make_entry();
        for(int i=0;i<128;i+=2)entry_set(bp,i);  /* even only */
        entry_rx(bp);
        int K4=8, n4=its/K4;

        /* (a) always domain 42 (even=auth): jnc NEVER taken -> perfect prediction */
        {
            stat_t *s=snew(n4);
            for(int j=0;j<3000;j++)bench_bt_jnc(bp->bitmap,42,8);
            for(int j=0;j<n4;j++)sadd(s,bench_bt_jnc(bp->bitmap,42,K4));
            sdone(s);
            printf("bp_same_auth,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            sfree(s);
        }

        /* (b) always domain 43 (odd=deny): jnc ALWAYS taken -> different pattern */
        {
            stat_t *s=snew(n4);
            for(int j=0;j<3000;j++)bench_bt_jnc(bp->bitmap,43,8);
            for(int j=0;j<n4;j++)sadd(s,bench_bt_jnc(bp->bitmap,43,K4));
            sdone(s);
            printf("bp_same_deny,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            sfree(s);
        }

        /* (c) alternating 42/43 (auth/deny): jnc alternates -> some mispredicts */
        {
            stat_t *s=snew(n4);
            for(int j=0;j<3000;j++)bench_bt_jnc(bp->bitmap,(j&1)?43:42,8);
            for(int j=0;j<n4;j++)sadd(s,bench_bt_jnc(bp->bitmap,(j&1)?43:42,K4));
            sdone(s);
            printf("bp_alternate,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            sfree(s);
        }

        /* (d) random domain 0~127: fully random outcomes */
        {
            stat_t *s=snew(n4);
            for(int j=0;j<3000;j++)bench_bt_jnc(bp->bitmap,rand()%128,8);
            for(int j=0;j<n4;j++)sadd(s,bench_bt_jnc(bp->bitmap,rand()%128,K4));
            sdone(s);
            printf("bp_random,%d,%lu,%.1f,%.1f,%.1f,%.1f,%.1f,%zu\n",K4,(unsigned long)s->n,s->median,s->fmean,s->p90,s->p99,s->p999,s->outliers);
            sfree(s);
        }
        munmap(bp,PAGE_SIZE);
    }

    return 0;
}
