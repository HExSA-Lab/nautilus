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

#ifndef LGNCG_CG_DATA_H_INCLUDED
#define LGNCG_CG_DATA_H_INCLUDED

#include "vector.h"
#include "../../legion_runtime/legion.h"

#include <stdint.h>

namespace lgncg {

struct CGData {
    //residual vector
    Vector r;
    // preconditioned residual vector
    Vector z;
    // direction vector
    Vector p;
    // krylov vector
    Vector Ap;

    CGData(int64_t nRows,
           LegionRuntime::HighLevel::Context &ctx,
           LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        r.create<double>(nRows, ctx, lrt);
        z.create<double>(nRows, ctx, lrt);
        p.create<double>(nRows, ctx, lrt);
        Ap.create<double>(nRows, ctx, lrt);
    }

    void
    free(LegionRuntime::HighLevel::Context &ctx,
         LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        r.free(ctx, lrt);
        z.free(ctx, lrt);
        p.free(ctx, lrt);
        Ap.free(ctx, lrt);
    }
    /**
     * partitions all CG data members.
     */
    void
    partition(int64_t nParts,
              LegionRuntime::HighLevel::Context &ctx,
              LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        r.partition(nParts, ctx, lrt);
        z.partition(nParts, ctx, lrt);
        p.partition(nParts, ctx, lrt);
        Ap.partition(nParts, ctx, lrt);
    }

private:
    CGData(void);
};

}
#endif
