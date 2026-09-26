/*
an assembly runtime could quickly get out of hand
so I made a lightweight C runtime for the heap allocation
*/

#include <stddef.h>
#include <stdint.h>

/*
heap block header sits before the user pointer
user pointer = hdr + sizeof(BlockHeader)

layout is 16 bytes 
*/
typedef struct {
    size_t size;
    uint32_t flags;
    struct BlockHeader* next; // for gc later on
    uint32_t mark_bits; // for gc later on
} BlockHeader;

#define FLAG_IN_USE 0x1u // allocated to user code
#define FLAG_PINNED 0x2u // reserved, dont relocate during GC

// should match MMAP_BASE = 0x0400_0000 in memory code, otherwise memory is wasted at best or worlds will collide at worst
#define HEAP_LIMIT 0x04000000UL

static inline long sys_brk(unsigned long new_break) {
    register long a0 asm("a0") = (long) new_break; // arg0 is new_break
    register long a7 asm("a7") = (long) 214; // sysbrk 
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

static inline long sys_write(int fd, const void *buf, unsigned long n) {
    register long a0 asm("a0") = fd;
    register long a1 asm("a1") = (long) buf;
    register long a2 asm("a2") = (long) n;
    register long a7 asm("a7") = 64;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}

void print_string(const char *str) {
    const char *p = str;
    while (*p) p++;
    sys_write(1, str, (unsigned long)(p - str));
    sys_write(1, "\n", 1);
}

static char* g_brk = 0; // cached brk
static int g_heap_ready = 0;

static void heap_ensure_init() {
    if (g_heap_ready) return;
    long cur = sys_brk(0);
    g_brk = (char*)(cur > 0 ? cur : 0);
    
    print_string("ensured init of heap");
    g_heap_ready = 1;
}

// i should later add g_free_list 

// start of ABI

void *__alloc(size_t size) {
    heap_ensure_init();
    if (g_brk == 0) return 0; // fail
    if (size == 0) size = 1;

    print_string("allocating...");

    size_t total = size + sizeof(BlockHeader);
    total = (total + 7u) & ~(size_t)7u; // 8-byte align

    char* old_brk = g_brk;
    char* new_brk = old_brk + total;
    if ((unsigned long) new_brk > HEAP_LIMIT) return 0;
    
    if (sys_brk((unsigned long) new_brk) != 0) return 0;
    g_brk = new_brk;

    BlockHeader *hdr = (BlockHeader*) old_brk;
    hdr->size = size;
    hdr->flags = FLAG_IN_USE;
    hdr->next = 0;
    hdr->mark_bits = 0;

    return (char*) hdr + sizeof(BlockHeader);
}

void __free(void *ptr) {
    if (!ptr) return;
    BlockHeader *hdr = (BlockHeader*)((char*) ptr - sizeof(BlockHeader));
    hdr->flags &= ~FLAG_IN_USE;

    // this actually leaks memory, just marks memory not in use, until we implement a free list
}

void *__realloc(void *ptr, size_t new_size) {
    if (!ptr) return __alloc(new_size);
    if (new_size ==0) { __free(ptr); return 0; }

    BlockHeader* hdr = (BlockHeader*)((char*) ptr - sizeof(BlockHeader));
    if (hdr->size >= new_size) return ptr;

    void *np = __alloc(new_size);
    if (!np) return 0;

    char *dst = (char*) np;
    char *src = (char*) ptr;

    for (size_t i = 0; i < hdr->size; i++) dst[i] = src[i];

    __free(ptr);
    return np;
}

void *__calloc(size_t n, size_t size) {
    size_t total = n * size;
    void *p = __alloc(total);
    if (!p) return 0;
    char *c = (char*) p;
    for (size_t i = 0; i < total; i++) c[i] = 0;
    return p;
}

// under freestanding following functions must be provided:
void *memcpy(void *dst, const void *src, size_t n) {
    char *d = (char*) dst;
    const char *s = (const char* )src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    char *d = (char*) dst;
    for (size_t i = 0; i < n; i++) d[i] = (char)c;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    char *d = (char* )dst;
    const char *s = (const char*) src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char*) a;
    const unsigned char *y = (const unsigned char*) b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

void __gc_collect(void) { /* nop */ }