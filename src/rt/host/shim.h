#include "../value.h"
#include "../obj.h"

#ifdef HOST_BUILD
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

Value __invoke_closure_2(Value closure, Value a1, Value a2) {
    Closure *c = (Closure *)PTR_VAL(closure);
    typedef Value (*Fn)(Value, Value, Value);
    return ((Fn)c->fn_ptr)(closure, a1, a2);
}

void *__alloc(uint32_t size) {
    if (size == 0) size = 1;
    void *p = calloc(1, size);
    if (!p) {
        fprintf(stderr, "host_shim: __alloc(%u) failed\n", size);
        abort();
    }
    return p;
}

void __free(void *p) {
    free(p);
}

__attribute__((noreturn))
void __panic(const char *msg) {
    fprintf(stderr, "panic: %s\n", msg);
    abort();
}
#endif