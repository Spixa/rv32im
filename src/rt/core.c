#include <stdint.h>
#include <stddef.h>

#include "value.h"
#include "obj.h"

// heap runtime
extern void *__alloc(uint32_t size);
extern void *__realloc(void* p, uint32_t size);
extern void __free(void* p);
extern void __panic(const char* msg); // noreturn

Value __make_closure(void* fn_ptr, uint32_t arity, uint32_t upval_count, const Value* upvals);
Value __build_partial(Value closure, uint32_t arity, const Value* args);
Value __pipe(Value right, Value subject);

Value __invoke_closure_1(Value closure, Value a1);
Value __invoke_closure_2(Value closure, Value a1, Value a2);
Value __invoke_closure_3(Value closure, Value a1, Value a2, Value a3);
Value __invoke_closure_4(Value closure, Value a1, Value a2, Value a3, Value a4);
Value __invoke_closure_5(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5);
Value __invoke_closure_6(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6);
Value __invoke_closure_7(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6, Value a7);

/*
codegen contract:
__make_closure allocates a Closure and copies the given upvalues into it (no moving)

how codegen uses it:
la a0, proto_N_label
li a1, arity
li a2, upval_count
mv a3, sp ; upval array which is the last element of the fn stack
jal ra, __make_closure
; result in a0
*/
Value __make_closure(void* fn_ptr, uint32_t arity, uint32_t upval_count, const Value* upvals) {
    uint32_t size = sizeof(Closure) + upval_count * sizeof(Value);
    Closure* c = (Closure*) __alloc(size);

    c->hdr.type = TAG_CLOSURE;
    c->hdr.flags = 0;
    c->hdr.refcount = 1;

    c->fn_ptr = fn_ptr;
    c->arity = arity; // new verity variant?
    c->upval_count = upval_count;

    for (uint32_t i = 0; i < upval_count; i++)
        c->upvals[i] = upvals[i];


    return MAKE_PTR(c);
}

Value __build_partial(Value closure, uint32_t arity, const Value* args) {
    uint32_t size = sizeof(Partial) + arity * sizeof(Value);
    Partial* p = (Partial*) __alloc(size);

    p->hdr.type = TAG_PARTIAL;
    p->hdr.flags = 0;
    p->hdr.refcount = 1;
    p->target = closure;
    p->arity = arity;

    uint32_t mask = 0;
    for (uint32_t i = 0; i < arity; i++) {
        p->args[i] = args[i];

        if (args[i] == HOLE_TAG) {
            mask |= (1u << 1);
        }
    }

    p->hole_mask = mask;
    return MAKE_PTR(p);
}


static Value __invoke_partial(Partial* p) {
    if (p->hole_mask != 0) {
        __panic("__invoke_partial: unfilled holes");
    }

    switch (p->arity) {
        case 1: return __invoke_closure_1(p->target, p->args[0]);
        case 2: return __invoke_closure_2(p->target, p->args[0], p->args[1]);
        case 3: return __invoke_closure_3(p->target, p->args[0], p->args[1], p->args[2]);
        case 4: return __invoke_closure_4(p->target, p->args[0], p->args[1], p->args[2], p->args[3]);
        case 5: return __invoke_closure_5(p->target, p->args[0], p->args[1], p->args[2], p->args[3], p->args[4]);
        case 6: return __invoke_closure_6(p->target, p->args[0], p->args[1], p->args[2], p->args[3], p->args[4], p->args[5]);
        case 7: return __invoke_closure_7(p->target, p->args[0], p->args[1], p->args[2], p->args[3], p->args[4], p->args[5], p->args[6]);
        default: {
            __panic("__invoke_partial: arity out of range");
            return NIL_VALUE;
        }
    }
}

Value __partial_fill(Value partial_v, Value subject) {
    if (!IS_PTR(partial_v)) {
        __panic("__partial_fill: not a partial");
    }

    Partial* p = (Partial*) PTR_VAL(partial_v);
    if (p->hdr.type != TAG_PARTIAL) {
        __panic("__partial_fill: wrong object type");
    }

    if (p->arity > 7) {
        __panic("__partial_fill: arity > 7 unsupported");
    }

    Value scratch[7];
    uint32_t new_mask = p->hole_mask;
    int filled = 0;

    for (uint32_t i = 0; i < p->arity; i++) {
        if (!filled && p->args[i] == HOLE_TAG) {
            scratch[i] =  subject;
            new_mask &= ~(1u << i);
            filled = 1;
        } else {
            scratch[i] = p->args[i];
        }
    }

    if (!filled) {
        __panic("__partial_fill: no holes to fill");
    }

    if (new_mask != 0) {
        // recursion, emulator supports recursion through stack pointer
        return __build_partial(p->target, p->arity, scratch);
    }

    // no holes left, invoke base case:
    switch (p->arity) {
        case 1: return __invoke_closure_1(p->target, scratch[0]);
        case 2: return __invoke_closure_2(p->target, scratch[0], scratch[1]);
        case 3: return __invoke_closure_3(p->target, scratch[0], scratch[1], scratch[2]);
        case 4: return __invoke_closure_4(p->target, scratch[0], scratch[1], scratch[2], scratch[3]);
        case 5: return __invoke_closure_5(p->target, scratch[0], scratch[1], scratch[2], scratch[3], scratch[4]);
        case 6: return __invoke_closure_6(p->target, scratch[0], scratch[1], scratch[2], scratch[3], scratch[4], scratch[5]);
        case 7: return __invoke_closure_7(p->target, scratch[0], scratch[1], scratch[2], scratch[3], scratch[4], scratch[5], scratch[6]);
        default: {
            __panic("__partial_fill: arity out of range");
            return NIL_VALUE;
        }
    }
}

Value __pipe(Value right, Value subject) {
    if (!IS_PTR(right)) {
        __panic("pipe: rhs is not pointer");
    }
    ObjectHeader* hdr = (ObjectHeader*) PTR_VAL(right);

    if (hdr->type == TAG_PARTIAL) {
        return __partial_fill(right, subject);
    }

    if (hdr->type == TAG_CLOSURE) {
        Closure* c = (Closure*) hdr; // this is so cool
        if (c->arity == 0) {
            __panic("pipe: closure has arity 0");
        }

        if (c->arity == 1) {
            return __invoke_closure_1(right, subject);
        }
        if (c->arity > 7) {
            __panic("pipe: closure arity > 7 unsupported");
        }

        Value scratch[7];
        scratch[0] = subject;
        for (uint32_t i = 1; i < c->arity; i++) {
            scratch[i] = HOLE_TAG;
        }
        return __build_partial(right, c->arity, scratch);
    }

    __panic("pipe: rhs is not callable");
    return NIL_VALUE;
}

#define INVOKE_BODY \
    "addi a0, a0, -1\n"    /* untag: a0 = raw Closure* */ \
    "lw   t0, 4(a0)\n"     /* t0 = fn_ptr */              \
    "jalr t0\n"            /* tail call; ra unchanged */


#ifndef HOST_BUILD
__attribute__((naked, used))
Value __invoke_closure_1(Value closure, Value a1) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_2(Value closure, Value a1, Value a2) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_3(Value closure, Value a1, Value a2, Value a3) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_4(Value closure, Value a1, Value a2, Value a3, Value a4) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_5(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_6(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

__attribute__((naked, used))
Value __invoke_closure_7(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6, Value a7) {
    __asm__ volatile(INVOKE_BODY ::: "memory");
}

#undef INVOKE_BODY
#else 
Value __invoke_closure_1(Value closure, Value a1) {}
// Value __invoke_closure_2(Value closure, Value a1, Value a2);
Value __invoke_closure_3(Value closure, Value a1, Value a2, Value a3) {}
Value __invoke_closure_4(Value closure, Value a1, Value a2, Value a3, Value a4) {}
Value __invoke_closure_5(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5) {}
Value __invoke_closure_6(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6) {}
Value __invoke_closure_7(Value closure, Value a1, Value a2, Value a3, Value a4, Value a5, Value a6, Value a7) {}
#endif

void __inc_ref(Value v)      { (void)v; }
void __dec_ref(Value v)      { (void)v; }
void __dec_ref_slot(Value v) { (void)v; }