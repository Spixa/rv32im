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
typedef struct BlockHeader {
    size_t size_flags;
    struct BlockHeader* prev_free;
    struct BlockHeader* next_free;
    uint32_t mark_bits; // for gc later on
} BlockHeader;

#define FLAG_IN_USE 0x1u // allocated to user code
#define FLAG_PINNED 0x2u // reserved, dont relocate during GC
#define FLAG_MASK   0x7u

#define HDR_SIZE    16u
#define FTR_SIZE    4u
#define OVERHEAD    (HDR_SIZE + FTR_SIZE)
#define MIN_BLOCK   24u
#define ALIGN8(n)   (((n) + 7u) & ~(size_t)7u)


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

#ifndef NDEBUG
void debug_str(const char *str) {
    const char *p = str;
    while (*p) p++;
    sys_write(1, str, (unsigned long)(p - str));
    sys_write(1, "\n", 1);
}
#else
#define debug_str(s) ((void)0)
#endif

static char* g_brk = 0; // cached brk
static char* g_heap_start = 0;
static int g_heap_ready = 0;
static BlockHeader* g_free_list;

// accessors
static inline size_t blk_size(BlockHeader* h) { return h->size_flags & ~FLAG_MASK; }
static inline int blk_in_use(BlockHeader* h) { return h->size_flags & FLAG_IN_USE; }

static inline void blk_set_size(BlockHeader* h, size_t s) {
    h->size_flags = s | (h->size_flags & FLAG_MASK);
}

static inline void blk_set_flags(BlockHeader* h, uint32_t f) {
    h->size_flags = (h->size_flags & ~FLAG_MASK) | (f & FLAG_MASK);
}

// must be called whenever blk_size changes so the next physical block can find us in the doubly linked list configuration
static inline void set_footer(BlockHeader* h) {
    *(uint32_t*)((char*) h + blk_size(h) - FTR_SIZE) = (uint32_t) blk_size(h);
}

// read the previous physical block's size from its footer
static inline size_t prev_blk_size(BlockHeader* h) {
    return *(uint32_t*)((char*) h - FTR_SIZE);
}

// free list impl
static void free_list_push(BlockHeader* h) {
    h->next_free = g_free_list;
    h->prev_free = 0;
    if (g_free_list) g_free_list->prev_free = h;
    g_free_list = h;
}

static void free_list_remove(BlockHeader* h) {
    // forward link
    if (h->prev_free) h->prev_free->next_free = h->next_free;
    else g_free_list = h->next_free;

    // backward link
    if (h->next_free) h->next_free->prev_free = h->prev_free;

    h->next_free = 0;
    h->prev_free = 0;
}

static void heap_ensure_init() {
    if (g_heap_ready) return;
    long cur = sys_brk(0);
    g_brk = (char*)(cur > 0 ? cur : 0);
    
    debug_str("heap initialized");
    g_heap_start = g_brk;
    g_heap_ready = 1;
}

// split an oversized block h into [h: need][r: rest], skip if no need
static void maybe_split(BlockHeader* h, size_t need) {
    size_t total = blk_size(h);
    if (total < need + MIN_BLOCK) return;

    size_t rest = total - need;
    blk_set_size(h, need); // we downsizing
    set_footer(h);

    BlockHeader* r = (BlockHeader*)((char*) h + need);
    r->size_flags = rest;
    r->next_free = 0;
    r->prev_free = 0;
    r->mark_bits = 0;
    set_footer(r);

    free_list_push(r);
}

// i should later add g_free_list 

// start of ABI

void *__alloc(size_t size) {
    heap_ensure_init();
    if (!g_brk) return 0; // fail

    if (size == 0) size = 1;
    size_t need = ALIGN8(size + OVERHEAD);
    if (need < MIN_BLOCK) need = MIN_BLOCK;

    // first look in the free list and see if anything fits
    for (BlockHeader* h = g_free_list; h; h = h->next_free) {
        if (blk_size(h) >= need) {
            free_list_remove(h);
            maybe_split(h, need);
            blk_set_flags(h, FLAG_IN_USE);
            h->mark_bits = 0;
            set_footer(h);
            debug_str("reused free block");
            return (char*) h + HDR_SIZE;
        }
    }


    char* old = g_brk;
    char* neu = old + need;
    if ((unsigned long) neu > HEAP_LIMIT) return 0; // OOM
    if (sys_brk((unsigned long) neu) != 0) return 0;
    g_brk = neu;

    BlockHeader* h = (BlockHeader*) old;
    h->size_flags = need | FLAG_IN_USE;
    h->next_free = 0;
    h->prev_free = 0;
    h->mark_bits = 0;
    set_footer(h);
    debug_str("bumped brk");

    return (char*) h + HDR_SIZE;
}

void __free(void *ptr) {
    if (!ptr) return;
    BlockHeader* h = (BlockHeader*)((char*) ptr - HDR_SIZE);
    if (!blk_in_use(h)) return; // double-free guard
    blk_set_flags(h, 0); // mark free

    // merge with next physical block if free
    char *next_addr = (char*) h + blk_size(h);
    if (next_addr < g_brk) {
        BlockHeader* next = (BlockHeader*) next_addr;

        if (!blk_in_use(next)) {
            free_list_remove(next);
            blk_set_size(h, blk_size(h) + blk_size(next));
        }
    }

    // merge with previous physical block if free
    if ((char*) h > g_heap_start) {
        size_t prev_size = prev_blk_size(h);
        BlockHeader* prev = (BlockHeader*)((char*) h - prev_size);
        if (!blk_in_use(prev)) {
            free_list_remove(prev);
            blk_set_size(prev, blk_size(prev) + blk_size(h));
            h = prev;
        }
    }

    set_footer(h);
    free_list_push(h);
    debug_str("freed (coalesced)");
}

void *__realloc(void *ptr, size_t new_size) {
    if (!ptr) return __alloc(new_size);
    if (new_size == 0) { __free(ptr); return 0; }

    BlockHeader* h = (BlockHeader*)((char*) ptr - HDR_SIZE);
    size_t old_user_size = blk_size(h) - OVERHEAD;
    if (old_user_size >= new_size) return ptr;

    void *np = __alloc(new_size);
    if (!np) return 0;

    char* dst = (char*) np;
    char* src = (char*) ptr;
    for (size_t i = 0; i < old_user_size; i++) dst[i] = src[i];

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