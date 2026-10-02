#include "../value.h"

#define HOST_BUILD
#include "shim.h"
#include "assert.h"

static Value add_impl(Value closure, Value a, Value b) {
    return MAKE_INT(INT_VAL(a) + INT_VAL(b));
}

Value __make_closure(void* fn_ptr, uint32_t arity, uint32_t upval_count, const Value* upvals);
Value __build_partial(Value closure, uint32_t arity, const Value* args);
Value __pipe(Value right, Value subject);

int main(void) {
    Value upvals[0];
    Value add = __make_closure((void*) add_impl, 2, 0, upvals);

    Value r1 = __invoke_closure_2(add, MAKE_INT(3), MAKE_INT(5));
    assert(INT_VAL(r1) == 8);
}