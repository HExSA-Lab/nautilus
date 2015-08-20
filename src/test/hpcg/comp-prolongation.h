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

#ifndef LGNCG_COMP_PROLONGATION_H_INCLUDED
#define LGNCG_COMP_PROLONGATION_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "tids.h"

#include "legion.h"

/**
 * computes the coarse residual vector.
 */

namespace lgncg {

/**
 * responsible for setting up the task launch of the compute prolongation
 * operation.
 */
static inline void
prolongation(SparseMatrix &Af,
             Vector &xf,
             LegionRuntime::HighLevel::Context &ctx,
             LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    // no per-task arguments. CGTaskArgs has task-specific info.
    ArgumentMap argMap;
    IndexLauncher il(LGNCG_PROLONGATION_TID, xf.lDom(),
                     TaskArgument(NULL, 0), argMap);
    // xf
    il.add_region_requirement(
        RegionRequirement(xf.lp(), 0, READ_WRITE, EXCLUSIVE, xf.lr)
    );
    il.add_field(idx++, xf.fid);
    // Af.mgData->xc
    il.add_region_requirement(
        RegionRequirement(Af.mgData->xc.lp(), 0, READ_ONLY,
                          EXCLUSIVE, Af.mgData->xc.lr)
    );
    il.add_field(idx++, Af.mgData->xc.fid);
    // Af.mgData->f2cOp
    il.add_region_requirement(
        RegionRequirement(Af.mgData->f2cOp.lp(), 0, READ_ONLY,
                          EXCLUSIVE, Af.mgData->f2cOp.lr)
    );
    il.add_field(idx++, Af.mgData->f2cOp.fid);
    // execute the thing...
    (void)lrt->execute_index_space(ctx, il);
}

/**
 *
 */
inline void
prolongationTask(
    const LegionRuntime::HighLevel::Task *task,
    const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
    LegionRuntime::HighLevel::Context ctx,
    LegionRuntime::HighLevel::HighLevelRuntime *lrt
)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx; (void)lrt;
    static const uint8_t xfRID  = 0;
    static const uint8_t xcRID  = 1;
    static const uint8_t f2cRID = 2;
    assert(3 == rgns.size());
    // name the regions
    const PhysicalRegion &xfpr  = rgns[xfRID];
    const PhysicalRegion &xcpr  = rgns[xcRID];
    const PhysicalRegion &f2cpr = rgns[f2cRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t> GLRA;
    // vectors
    GDRA xf  = xfpr.get_field_accessor(0).typeify<double>();
    const Domain xfDom = lrt->get_index_space_domain(
        ctx, task->regions[xfRID].region.get_index_space()
    );
    GDRA xc  = xcpr.get_field_accessor(0).typeify<double>();
    const Domain xcDom = lrt->get_index_space_domain(
        ctx, task->regions[xcRID].region.get_index_space()
    );
    GLRA f2c = f2cpr.get_field_accessor(0).typeify<int64_t>();
    const Domain f2cDom = lrt->get_index_space_domain(
        ctx, task->regions[f2cRID].region.get_index_space()
    );
    // xf
    Rect<1> myGridBounds = xfDom.get_rect<1>();
    Rect<1> rec; ByteOffset boff[1];
    double *xfp = xf.raw_rect_ptr<1>(myGridBounds, rec, boff);
    bool offd = offsetsAreDense<1, double>(myGridBounds, boff);
    assert(offd);
    // xc
    myGridBounds = xcDom.get_rect<1>();
    const double *const xcp = xc.raw_rect_ptr<1>(myGridBounds, rec, boff);
    offd = offsetsAreDense<1, double>(myGridBounds, boff);
    assert(offd);
    // f2c
    myGridBounds = f2cDom.get_rect<1>();
    Rect<1> f2csr; ByteOffset f2cOff[1];
    const int64_t *const f2cp = f2c.raw_rect_ptr<1>(myGridBounds,
                                                    f2csr, f2cOff);
    offd = offsetsAreDense<1, int64_t>(myGridBounds, f2cOff);
    assert(offd);
    // now, actually perform the computation
    const int64_t nc = xcDom.get_rect<1>().volume(); // length of xc
    //
    for (int64_t i = 0; i < nc; ++i) {
        xfp[f2cp[i]] += xcp[i];
    }
}

}

#endif
