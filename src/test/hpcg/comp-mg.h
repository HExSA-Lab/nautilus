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

#ifndef LGNCG_COMP_MG_H_INCLUDED
#define LGNCG_COMP_MG_H_INCLUDED

#include "tids.h"
#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "vec-zero.h"
#include "comp-spmv.h"
#include "comp-symgs.h"
#include "comp-restriction.h"
#include "comp-prolongation.h"

namespace lgncg {

static inline void
mgPrep(SparseMatrix &A,
       Vector &r,
       Vector &x,
       LegionRuntime::HighLevel::Context &ctx,
       LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    // recursively push partition schemes on the target data structures.
    if (A.mgData) {
        //A.mgData->Axf.partition(A.nParts, ctx, lrt); // already partitioned
        r.partition(A.nParts, ctx, lrt);
        x.partition(A.nParts, ctx, lrt);
        //A.Ac->partition(A.geom, ctx, lrt); // already partitioned
        A.mgData->rc.partition(A.nParts, ctx, lrt);
        A.mgData->xc.partition(A.nParts, ctx, lrt);
        // now do the same for the next coarsest level
        mgPrep(*A.Ac, A.mgData->rc, A.mgData->xc, ctx, lrt);
    }
}

static inline void
mg(SparseMatrix &A,
   Vector &r,
   Vector &x,
   LegionRuntime::HighLevel::Context &ctx,
   LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    // zero out x
    veczero(x, ctx, lrt);
    // go to the next coarsest level if available
    if (A.mgData) {
        const int64_t nPresmootherSteps = A.mgData->nPresmootherSteps;
        for (int64_t i = 0; i < nPresmootherSteps; ++i) {
            symgs(A, x, r, ctx, lrt);
        }
        spmv(A, x, A.mgData->Axf, ctx, lrt);
        // perform restriction operation using simple injection
        restriction(A, r, ctx, lrt);
        mg(*A.Ac, A.mgData->rc, A.mgData->xc, ctx, lrt);
        prolongation(A, x, ctx, lrt);
        const int64_t nPostsmootherSteps = A.mgData->nPostsmootherSteps;
        for (int64_t i = 0; i < nPostsmootherSteps; ++i) {
            symgs(A, x, r, ctx, lrt);
        }
    }
    else {
        symgs(A, x, r, ctx, lrt);
    }
}

}

#endif
