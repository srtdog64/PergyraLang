#ifndef PGY_RUNTIME_LIB_CHECKED_ARITH_CORE_H
#define PGY_RUNTIME_LIB_CHECKED_ARITH_CORE_H

#include <stdint.h>
#include <limits.h>

#include "pgy_runtime_panic_contract.h"

int32_t
pgy_checked_div_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    if (lhs == INT32_MIN && rhs == -1)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_DIVISION_OVERFLOW);
    return lhs / rhs;
}

int64_t
pgy_checked_div_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    if (lhs == INT64_MIN && rhs == -1)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_DIVISION_OVERFLOW);
    return lhs / rhs;
}

int32_t
pgy_checked_mod_i32_export(int32_t lhs, int32_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    /* INT_MIN % -1 is UB in C though the true remainder is 0; return it. */
    if (lhs == INT32_MIN && rhs == -1)
        return 0;
    return lhs % rhs;
}

int64_t
pgy_checked_mod_i64_export(int64_t lhs, int64_t rhs)
{
    if (rhs == 0)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_DIVIDE_BY_ZERO,
                          PGY_RUNTIME_PANIC_REASON_DIVIDE_BY_ZERO);
    /* INT_MIN % -1 is UB in C though the true remainder is 0; return it. */
    if (lhs == INT64_MIN && rhs == -1)
        return 0;
    return lhs % rhs;
}

int32_t
pgy_checked_add_i32_export(int32_t lhs, int32_t rhs)
{
    if (((rhs > 0) && (lhs > INT32_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT32_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

int64_t
pgy_checked_add_i64_export(int64_t lhs, int64_t rhs)
{
    if (((rhs > 0) && (lhs > INT64_MAX - rhs)) ||
        ((rhs < 0) && (lhs < INT64_MIN - rhs)))
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_ADDITION_OVERFLOW);
    return lhs + rhs;
}

int32_t
pgy_checked_mul_i32_export(int32_t lhs, int32_t rhs)
{
    int overflow = 0;
    if (lhs > 0) {
        if (rhs > 0) { overflow = (lhs > INT32_MAX / rhs); }
        else         { overflow = (rhs < INT32_MIN / lhs); }
    } else {
        if (rhs > 0) { overflow = (lhs < INT32_MIN / rhs); }
        else         { overflow = (lhs != 0 && rhs < INT32_MAX / lhs); }
    }
    if (overflow)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_MULTIPLICATION_OVERFLOW);
    return lhs * rhs;
}

int64_t
pgy_checked_mul_i64_export(int64_t lhs, int64_t rhs)
{
    int overflow = 0;
    if (lhs > 0) {
        if (rhs > 0) { overflow = (lhs > INT64_MAX / rhs); }
        else         { overflow = (rhs < INT64_MIN / lhs); }
    } else {
        if (rhs > 0) { overflow = (lhs < INT64_MIN / rhs); }
        else         { overflow = (lhs != 0 && rhs < INT64_MAX / lhs); }
    }
    if (overflow)
        PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_ARITHMETIC_OVERFLOW,
                          PGY_RUNTIME_PANIC_REASON_MULTIPLICATION_OVERFLOW);
    return lhs * rhs;
}

#endif /* PGY_RUNTIME_LIB_CHECKED_ARITH_CORE_H */
