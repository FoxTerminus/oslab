#include "mymalloc.h"

#define PG_SIZE (1 << 12)

#define SZ 10007
long malloc_count = 0;

static inline int gettid(void) {
    int tid;
    __asm__ volatile (
        "syscall"
        : "=a" (tid)
        : "0" (186)
        : "rcx", "r11", "memory"
    );
    return tid;
}

uint64_t siz[SZ];
void *addr[SZ];

struct Block *head[SZ] = {0};

void *mymalloc(size_t size) {
    size = ((size >> 3) + 1) << 3;
    int asize = ((size >> 12) + 1) << 12;
    int tid = gettid() % SZ;
    if (siz[tid] < size) {
        addr[tid] = vmalloc(NULL, asize);
        siz[tid] = asize;
    }
    void *res = addr[tid];
    addr[tid] += size;
    siz[tid] -= size;
    return res;
}

void myfree(void *ptr) {
    return;
}
