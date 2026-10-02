#pragma once
#include <stdint.h>
#include <stddef.h>

#include "value.h"

#define TAG_STRING    1
#define TAG_LIST      2
#define TAG_RECORD    3
#define TAG_CLOSURE   4
#define TAG_PARTIAL   5
#define TAG_VARIANT   6
#define TAG_FLOAT     7

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t refcount;
} ObjectHeader;

/*
example:

add := |a: int, b: int| -> a + b;
add(2, 3); 
*/
typedef struct {
    ObjectHeader hdr; // offset 0
    void* fn_ptr; // offset 4
    uint32_t arity; // offset 8
    uint32_t upval_count; // offset 12
    Value upvals[]; // offset 16
} Closure;

/*
example:

add := |c: int| map(_, |n| n + c);
[1, 2, 3] |> add(10);
*/
typedef struct {
    ObjectHeader hdr; // offset 0
    Value target; // offset 4
    uint32_t arity; // offset 8
    uint32_t hole_mask; // offset 12
    Value args[]; // ofset 16
} Partial;

_Static_assert(offsetof(Closure, fn_ptr) == 4, "Closure.fn_ptr offset");
_Static_assert(offsetof(Closure, arity) == 8, "Closure.arity offset");
_Static_assert(offsetof(Closure, upval_count) == 12, "Closure.upval_count offset");
_Static_assert(offsetof(Closure, upvals) == 16, "Closure.upvals offset");

_Static_assert(offsetof(Partial, target) == 4, "Partial.target offset");
_Static_assert(offsetof(Partial, arity) == 8, "Partial.arity offset");
_Static_assert(offsetof(Partial, hole_mask) == 12, "Partial.hole_mask offset");
_Static_assert(offsetof(Partial, args) == 16, "Partial.args offset");