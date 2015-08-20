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

#ifndef LGNCG_MG_DATA_H_INCLUDED
#define LGNCG_MG_DATA_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"

#include "../../legion_runtime/legion.h"

namespace lgncg {

struct MGData {
    // call comp-symgs this many times prior to coarsening
    int64_t nPresmootherSteps;
    // call comp-symgs this many times after coarsening
    int64_t nPostsmootherSteps;
    // 1d array containing the fine operator local ids that will be injected
    // into coarse space.
    // length: number of rows in fine grid matrix
    Vector f2cOp;
    // coarse grid residual vector.
    // length: cnx * cny * cnz (coarse number of rows)
    Vector rc;
    // coarse grid solution vector
    // length: (coarse number of columns (not real number of columns -- e.g.
    // stencil size)). since we are dealing with square matrices, then that
    // corresponds to the coarse number of rows. so, cnx * cny * cnz
    Vector xc;
    // fine grid residual vector
    // length: fnx * fny * fnz (see: comments about xc -- similar thing here,
    // but for the fine grid matrix).
    Vector Axf;

    MGData(void)
    {
        nPresmootherSteps = 0;
        nPostsmootherSteps = 0;
    }

    MGData(int64_t nFineRows,
           int64_t nCoarseRows,
           LegionRuntime::HighLevel::Context &ctx,
           LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        // must be the same
        nPresmootherSteps = nPostsmootherSteps = 1;
        // XXX - in HPCG the length of f2cOp is nFineRows, but I don't think
        // that's needed -- at least based on how it's used in this code.
        f2cOp.create<int64_t>(nCoarseRows, ctx, lrt);
        // about the size here: read comment above.
        Axf.create<double>(nFineRows, ctx, lrt);
        rc.create<double>(nCoarseRows, ctx, lrt);
        // about the size here: read comment above.
        xc.create<double>(nCoarseRows, ctx, lrt);
    }

    void
    free(LegionRuntime::HighLevel::Context &ctx,
         LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        f2cOp.free(ctx, lrt);
        rc.free(ctx, lrt);
        xc.free(ctx, lrt);
        Axf.free(ctx, lrt);
    }

    void
    partition(int64_t nParts,
              LegionRuntime::HighLevel::Context &ctx,
              LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        f2cOp.partition(nParts, ctx, lrt);
        rc.partition(nParts, ctx, lrt);
        xc.partition(nParts, ctx, lrt);
        Axf.partition(nParts, ctx, lrt);
    }
};

}

#endif
