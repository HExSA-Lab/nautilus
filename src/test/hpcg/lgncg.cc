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

#include "lgncg.h"
#include "cg.h"

#include "legion.h"

#include <stdint.h>

namespace lgncg {

void
init(void)
{
    lgncg::cg::init();
}

void
finalize(void) { }

/**
 * simple interface to real call.
 */
void
cgSolv(SparseMatrix &A,
       Vector &b,
       double tolerance,
       int64_t maxIters,
       Vector &x,
       bool doPreconditioning,
       LegionRuntime::HighLevel::Context ctx,
       LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    lgncg::cg::solv(A, b, tolerance, maxIters, x,
                    doPreconditioning, ctx, lrt);
}

std::ostream &
operator<<(std::ostream &os, const I64Tuple &tup) {
    os << '(' << tup.a << ", " << tup.b << ')';
    return os;
}

} // end lgncg namespace
