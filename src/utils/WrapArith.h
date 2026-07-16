#ifndef WRAP_ARITH_H
#define WRAP_ARITH_H

#include <limits>

// ── EZ's integer arithmetic semantics, in one place ──────────────────────────
//
// EZ deliberately uses wrapping (two's-complement) integers: the math library's
// LCG relies on large multiplications wrapping before a `%`, and hash/modulo
// chains depend on the same behaviour.
//
// Signed overflow is undefined behaviour in C++ though, so doing it directly on
// `long long` lets the optimiser assume it never happens. Performing the op on
// the unsigned representation is fully defined and produces exactly the wrap the
// language intends.
//
// These live in a shared header because BOTH the VM and the compiler's constant
// folder must agree: if they diverge, an expression silently means something
// different depending on whether it happened to be folded at compile time.
namespace ezarith {

inline long long wrapAdd(long long a, long long b) {
    return (long long)((unsigned long long)a + (unsigned long long)b);
}
inline long long wrapSub(long long a, long long b) {
    return (long long)((unsigned long long)a - (unsigned long long)b);
}
inline long long wrapMul(long long a, long long b) {
    return (long long)((unsigned long long)a * (unsigned long long)b);
}
inline long long wrapNeg(long long a) {
    return (long long)(0ULL - (unsigned long long)a);
}

// True when `a / b` (and `a % b`) would be undefined behaviour: division by
// zero, or LLONG_MIN / -1 whose quotient is not representable.
inline bool divIsUB(long long a, long long b) {
    return b == 0 || (a == std::numeric_limits<long long>::min() && b == -1);
}

// EZ's division rule: an integer division stays an integer only when it divides
// exactly, otherwise it promotes to double (so `5 / 2` is 2.5, not 2).
// Callers must have already rejected divIsUB().
inline bool divIsExact(long long a, long long b) {
    return a % b == 0;
}

} // namespace ezarith

#endif // WRAP_ARITH_H
