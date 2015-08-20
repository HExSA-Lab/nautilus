/**
 * Copyright (c) 2014      Los Alamos National Security, LLC
 *                         All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * LA-CC 10-123
 */

#ifndef LGNCG_DOTPROD_ACCUMULATE_H_INCLUDED
#define LGNCG_DOTPROD_ACCUMULATE_H_INCLUDED

#include "legion.h"

namespace lgncg {
/**
 *
 */
class DotProdAccumulate {
public:
    typedef double LHS;
    typedef double RHS;
    static const double identity;

    template <bool EXCLUSIVE>
    static void apply(LHS &lhs, RHS rhs);

    template <bool EXCLUSIVE>
    static void fold(RHS &rhs1, RHS rhs2);

};

const double DotProdAccumulate::identity = 0.0;

template<>
void
DotProdAccumulate::apply<true>(LHS &lhs, RHS rhs) {
    lhs += rhs;
}

template<>
void
DotProdAccumulate::apply<false>(LHS &lhs, RHS rhs) {
    int64_t *target = (int64_t *)&lhs;
    union { int64_t as_int; double as_T; } oldval, newval;
    do {
        oldval.as_int = *target;
        newval.as_T = oldval.as_T + rhs;
    } while (!__sync_bool_compare_and_swap(target, oldval.as_int, newval.as_int));
}

template<>
void
DotProdAccumulate::fold<true>(RHS &rhs1, RHS rhs2) {
    rhs1 += rhs2;
}

template<>
void
DotProdAccumulate::fold<false>(RHS &rhs1, RHS rhs2) {
    int64_t *target = (int64_t *)&rhs1;
    union { int64_t as_int; double as_T; } oldval, newval;
    do {
        oldval.as_int = *target;
        newval.as_T = oldval.as_T + rhs2;
    } while (!__sync_bool_compare_and_swap(target, oldval.as_int, newval.as_int));
}

}

#endif
