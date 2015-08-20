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

#ifndef LGNCG_COMP_RESTRICTION_H_INCLUDED
#define LGNCG_COMP_RESTRICTION_H_INCLUDED

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
 * responsible for setting up the task launch of the compute restriction
 * operation.
 */
static inline void
restriction(SparseMatrix &A,
            Vector &rf,
            LegionRuntime::HighLevel::Context &ctx,
            LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    ArgumentMap argMap;
    IndexLauncher il(LGNCG_RESTRICTION_TID, A.mgData->Axf.lDom(),
                     TaskArgument(NULL, 0), argMap);
    // A.mgData->Axf
    il.add_region_requirement(
        RegionRequirement(A.mgData->Axf.lp(), 0, READ_ONLY,
                          EXCLUSIVE, A.mgData->Axf.lr)
    );
    il.add_field(idx++, A.mgData->Axf.fid);
    // rf
    il.add_region_requirement(
        RegionRequirement(rf.lp(), 0, READ_ONLY, EXCLUSIVE, rf.lr)
    );
    il.add_field(idx++, rf.fid);
    // A.mgData->rc
    il.add_region_requirement(
        RegionRequirement(A.mgData->rc.lp(), 0, WRITE_DISCARD,
                          EXCLUSIVE, A.mgData->rc.lr)
    );
    il.add_field(idx++, A.mgData->rc.fid);
    // A.mgData->f2cOp
    il.add_region_requirement(
        RegionRequirement(A.mgData->f2cOp.lp(), 0, READ_ONLY,
                          EXCLUSIVE, A.mgData->f2cOp.lr)
    );
    il.add_field(idx++, A.mgData->f2cOp.fid);
    // execute the thing...
    (void)lrt->execute_index_space(ctx, il);
}

/**
 *
 */
inline void
restrictionTask(
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
    static const uint8_t AxfRID  = 0;
    static const uint8_t rfRID   = 1;
    static const uint8_t rcRID   = 2;
    static const uint8_t f2cRID  = 3;
    // A.mgData->Axf, rf, A.mgData->rc, A.mgData->f2cOp
    assert(4 == rgns.size());
    // name the regions
    const PhysicalRegion &Axfpr = rgns[AxfRID];
    const PhysicalRegion &rfpr  = rgns[rfRID];
    const PhysicalRegion &rcpr  = rgns[rcRID];
    const PhysicalRegion &f2cpr = rgns[f2cRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t> GLRA;
    // vectors
    GDRA Axf = Axfpr.get_field_accessor(0).typeify<double>();
    const Domain AxfDom = lrt->get_index_space_domain(
        ctx, task->regions[AxfRID].region.get_index_space()
    );
    GDRA rf  = rfpr.get_field_accessor(0).typeify<double>();
    const Domain rfDom = lrt->get_index_space_domain(
        ctx, task->regions[rfRID].region.get_index_space()
    );
    GDRA rc  = rcpr.get_field_accessor(0).typeify<double>();
    const Domain rcDom = lrt->get_index_space_domain(
        ctx, task->regions[rcRID].region.get_index_space()
    );
    GLRA f2c = f2cpr.get_field_accessor(0).typeify<int64_t>();
    const Domain f2cDom = lrt->get_index_space_domain(
        ctx, task->regions[f2cRID].region.get_index_space()
    );
    Rect<1> myGridBounds = AxfDom.get_rect<1>();
    Rect<1> rec; ByteOffset boff[1];
    // Axf
    const double *const Axfp = Axf.raw_rect_ptr<1>(myGridBounds, rec, boff);
    bool offd = offsetsAreDense<1, double>(myGridBounds, boff);
    assert(offd);
    // rf
    myGridBounds = rfDom.get_rect<1>();
    const double *const rfp = rf.raw_rect_ptr<1>(myGridBounds, rec, boff);
    offd = offsetsAreDense<1, double>(myGridBounds, boff);
    assert(offd);
    // rc
    myGridBounds = rcDom.get_rect<1>();
    double *rcp = rc.raw_rect_ptr<1>(myGridBounds, rec, boff);
    offd = offsetsAreDense<1, double>(myGridBounds, boff);
    assert(offd);
    // f2c
    myGridBounds = f2cDom.get_rect<1>();
    Rect<1> f2csr; ByteOffset f2cOff[1];
    const int64_t *const f2cp = f2c.raw_rect_ptr<1>(myGridBounds,
                                                    f2csr, f2cOff);
    offd = offsetsAreDense<1, int64_t>(myGridBounds, boff);
    assert(offd);
    // now, actually perform the computation
    const int64_t nc = rcDom.get_rect<1>().volume();
    for (int64_t i = 0; i < nc; ++i) {
        rcp[i] = rfp[f2cp[i]] - Axfp[f2cp[i]];
    }
}

}

#endif
