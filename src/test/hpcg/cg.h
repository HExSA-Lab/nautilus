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

#ifndef LGNCG_CG_H_INCLUDED
#define LGNCG_CG_H_INCLUDED

#include "tids.h"
#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "cg-data.h"
#include "cg-mapper.h"
#include "veccp.h"
#include "vec-zero.h"
#include "comp-spmv.h"
#include "comp-waxpby.h"
#include "comp-dotprod.h"
#include "comp-mg.h"
#include "comp-symgs.h"
#include "setup-halo.h"

#include "legion.h"

#include <stdint.h>
#include <inttypes.h>
#include <cmath>

// FIXME HACK
#ifndef PRId64
#define PRId64 "lld"
#endif

namespace {

void
preSolvPrep(lgncg::SparseMatrix &A,
            lgncg::Vector &b,
            lgncg::Vector &x,
            lgncg::CGData &cgData,
            int64_t nParts,
            bool doMGPrecondPrep,
            LegionRuntime::HighLevel::Context &ctx,
            LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    assert(A.nRows == x.len && x.len == b.len);
    cgData.partition(nParts, ctx, lrt);
    // and if we are going to go down the MG route, setup partitions at all
    // levels before we start the preconditioning. we are hoisting this out of
    // the performance critical path. so, for each "new" call to mg, we must do
    // this so that all the data structures will be prepped.
    if (doMGPrecondPrep) {
        lgncg::Vector &r = cgData.r;
        lgncg::Vector &z = cgData.z;
        mgPrep(A, r, z, ctx, lrt);
    }
}

} // end namespace

namespace lgncg {
namespace cg {

static inline void
finalize(void) { }

typedef unsigned char uchar_t;
#define rdtscll(val)                    \
    do {                        \
    uint64_t tsc;                   \
    uint32_t a, d;                  \
    asm volatile("rdtsc" : "=a" (a), "=d" (d): : "memory"); \
    *(uint32_t *)&(tsc) = a;            \
    *(uint32_t *)(((uchar_t *)&tsc) + 4) = d;   \
    val = tsc;                  \
    } while (0)

/**
 * interface that performs the CG solve.
 */
static inline void
solv(SparseMatrix &A,
     Vector &b,
     double tolerance,
     int64_t maxIters,
     Vector &x,
     bool doPreconditioning,
     LegionRuntime::HighLevel::Context &ctx,
     LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    // the 2-norm of the residual vector after the last iteration
    double normr = 0.0;
    // initial residual for convergence testing
    double normr0 = 0.0;
    double rtz = 0.0, rtzOld = 0.0;
    double alpha = 0.0, beta = 0.0;
    double pAp = 0.0;
    // A, x, and b all have the same number of rows, so choose one
    CGData cgData(A.nRows, ctx, lrt);
    // performs all prep on the data structures we are going to work on
    preSolvPrep(A, b, x, cgData, A.geom.size, doPreconditioning, ctx, lrt);
    // convenience refs to CG data
    Vector &r  = cgData.r;
    Vector &z  = cgData.z;
    Vector &p  = cgData.p;
    Vector &Ap = cgData.Ap;
    if (!doPreconditioning) {
        printf("*** WARNING: PERFORMING UNPRECONDITIONED ITERATIONS ***\n");
    }
    // p = x
    veccp(x, p, ctx, lrt);
    // Ap = A * p
    spmv(A, p, Ap, ctx, lrt);
    // r = b - Ax
    waxpby(1.0, b, -1.0, Ap, r, ctx, lrt);
    // normr = r' * r
    dotprod(r, r, normr, ctx, lrt);
    normr = sqrt(normr);
    //printf("  . initial residual = %lf\n", normr);
    // record initial residual for convergence testing
    normr0 = normr;
#if 1
    for (int64_t k = 1; k <= maxIters && (normr / normr0 > tolerance); ++k) {
		unsigned long start, stop; 
		rdtscll(start);
        if (doPreconditioning) {
            mg(A, r, z, ctx, lrt);
        }
        else {
            // copy r to z - no preconditioning (could also use veccp, but this
            // is the way HPCG does it...
            waxpby(1.0, r, 0.0, r, z, ctx, lrt);
        }
		rdtscll(stop);

		//printf("Solv: first step = %llu cycles\n", stop-start);

		rdtscll(start);
        if (1 == k) {
            // p = z
            veccp(z, p, ctx, lrt);
            // rtz = r' * z
            dotprod(r, z, rtz, ctx, lrt);
        }
        else {
            rtzOld = rtz;
            // rtz = r' * z
            dotprod(r, z, rtz, ctx, lrt);
            beta = rtz / rtzOld;
            // p = 1 * z + beta * p
            waxpby(1.0, z, beta, p, p, ctx, lrt);
        }
		rdtscll(stop);

		//printf("Solv: 2nd step = %llu cycles\n", stop-start);

		rdtscll(start);
        // Ap = A * p
        spmv(A, p, Ap, ctx, lrt);
        // pAp = p' * Ap
        dotprod(p, Ap, pAp, ctx, lrt);
        alpha = rtz / pAp;
        // x = 1 * x + alpha * p
        waxpby(1.0, x, alpha, p, x, ctx, lrt);
        // r = 1 * r + -alpha * p
        waxpby(1.0, r, -alpha, Ap, r, ctx, lrt);
        // normr = r' * r
        dotprod(r, r, normr, ctx, lrt);
        normr = sqrt(normr);
		rdtscll(stop);
		//printf("Solv: 3rd step = %llu cycles\n", stop-start);
        //printf("  . iteration = %03"PRId64
               //" scaled residual = %ld\n", k, (long)(normr / normr0));
    }
#else // TESTING ONLY
    printf("*** WARNING: SYMGS-ONLY SOLVE ***\n");
    // this is just for unit testing purposes, so just run maxIters.
    for (int64_t k = 1; k <= maxIters; ++k) {
        lgncg::symgs(A, x, b, ctx, lrt);
    }
#endif
    // all done, free the resources allocated to perform this computation
    cgData.free(ctx, lrt);
}

static inline void
init(void)
{
    using namespace LegionRuntime::HighLevel;
    // register custom mapper
    HighLevelRuntime::set_registration_callback(mapperRegistration);

    HighLevelRuntime::register_legion_task<setupHaloTask>(
        LGNCG_SETUP_HALO_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-setup-halo-task"
    );
#if 0 // Using CopyLauncher -- Much faster!
    HighLevelRuntime::register_legion_task<veccpTask>(
        LGNCG_VECCP_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-veccp-task"
    );
#endif
    HighLevelRuntime::register_legion_task<veczeroTask>(
        LGNCG_VEC_ZERO_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-veczero-task"
    );
    HighLevelRuntime::register_legion_task<spmvTask>(
        LGNCG_SPMV_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-spmv-task"
    );
    HighLevelRuntime::register_legion_task<waxpbyTask>(
        LGNCG_WAXPBY_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-waxpby-task"
    );
    HighLevelRuntime::register_legion_task<double, dotProdTask>(
        LGNCG_DOTPROD_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-dotprod-task"
    );
    HighLevelRuntime::register_reduction_op<DotProdAccumulate>(
        LGNCG_DOTPROD_RED_ID
    );
    HighLevelRuntime::register_legion_task<symgsTask>(
        LGNCG_SYMGS_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-symgs-task"
    );
    HighLevelRuntime::register_legion_task<restrictionTask>(
        LGNCG_RESTRICTION_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-restriction-task"
    );
    HighLevelRuntime::register_legion_task<prolongationTask>(
        LGNCG_PROLONGATION_TID /* task id */,
        Processor::LOC_PROC /* proc kind  */,
        true /* single */,
        true /* index */,
        AUTO_GENERATE_ID,
        TaskConfigOptions(true /* leaf task */),
        "lgncg-prolongation-task"
    );
}

} // end cg namespace
} // end lgncg namespace

#endif
