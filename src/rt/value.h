#pragma once
#include <stdint.h>

typedef uint32_t Value;

#define IS_INT(v) (((v) & 1u) == 0u)
#define IS_PTR(v) (((v) & 1u) == 1u)

#define MAKE_INT(n)  ((Value)(((int32_t)(n)) << 1))
#define INT_VAL(v)   (((int32_t)(v)) >> 1)
#define MAKE_PTR(p)  ((Value)(((uintptr_t)(p)) | 1u))
#define PTR_VAL(v)   ((void *)((uintptr_t)(v) & ~1u))

#define FALSE_VALUE  MAKE_INT(0)
#define TRUE_VALUE   MAKE_INT(1)
#define NIL_VALUE    MAKE_INT(2)
#define UNIT_VALUE   MAKE_INT(3)
#define HOLE_TAG     ((Value)0xFFFFFFFFu)

#define INT_MAX_TAGGED   ((1 << 30) - 1)
#define INT_MIN_TAGGED   (-(1 << 30))