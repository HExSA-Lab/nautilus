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

#ifndef LGNCG_COMP_WAXPBY_H_INCLUDED
#define LGNCG_COMP_WAXPBY_H_INCLUDED

#include "tids.h"
#include "vector.h"
#include "sparsemat.h"
#include "utils.h"

#include "legion.h"

namespace {

#define LGNCG_DENSE_WAXPBY_STRIP_SIZE 256

#define GDRA LegionRuntime::Accessor::RegionAccessor \
    <LegionRuntime::Accessor::AccessorType::Generic, double>

#define GLRA LegionRuntime::Accessor::RegionAccessor \
    <LegionRuntime::Accessor::AccessorType::Generic, int64_t>

struct waxpbyTaskArgs {
    double alpha;
    double beta;
    bool xySame;
    bool xwSame;
    waxpbyTaskArgs(double alpha,
                   double beta,
                   bool xySame,
                   bool xwSame)
        : alpha(alpha), beta(beta), xySame(xySame), xwSame(xwSame) { ; }
};

/**
 * w = alpha * x + beta * y
 */
bool
denseWAXPBY(const Rect<1> &subgridBounds,
            double alpha,
            double beta,
            GDRA &fax,
            GDRA &fay,
            GDRA &faw)
{
    using namespace lgncg;
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;

    Rect<1> subrect;
    ByteOffset inOffsets[1], offsets[1], woffsets[1];
    //
    const double *xp = fax.raw_rect_ptr<1>(subgridBounds, subrect, inOffsets);
    if (!xp || (subrect != subgridBounds) ||
        !offsetsAreDense<1, double>(subgridBounds, inOffsets)) return false;
    //
    double *yp = fay.raw_rect_ptr<1>(subgridBounds, subrect, offsets);
    if (!yp || (subrect != subgridBounds) ||
        offsetMismatch(1, inOffsets, offsets)) return false;
    //
    double *wp = faw.raw_rect_ptr<1>(subgridBounds, subrect, woffsets);
    if (!wp || (subrect != subgridBounds) ||
        !offsetsAreDense<1, double>(subgridBounds, woffsets)) return false;

    int npts = subgridBounds.volume();
    while (npts > 0) {
        if (npts >= LGNCG_DENSE_WAXPBY_STRIP_SIZE) {
            for (int i = 0; i < LGNCG_DENSE_WAXPBY_STRIP_SIZE; i++) {
                wp[i] = (alpha * xp[i]) + (beta * yp[i]);
            }
            npts -= LGNCG_DENSE_WAXPBY_STRIP_SIZE;
            xp   += LGNCG_DENSE_WAXPBY_STRIP_SIZE;
            yp   += LGNCG_DENSE_WAXPBY_STRIP_SIZE;
            wp   += LGNCG_DENSE_WAXPBY_STRIP_SIZE;
        } else {
            for (int i = 0; i < npts; i++) {
                wp[i] = (alpha * xp[i]) + (beta * yp[i]);
            }
            npts = 0;
        }
    }
    return true;
}

#undef GDRA
#undef GLRA

}

namespace lgncg {

/**
 * responsible for setting up the task launch of the waxpby.
 * TODO fix region aliasing...
 */
static inline void
waxpby(double alpha,
       Vector &x,
       double beta,
       Vector &y,
       Vector &w,
       LegionRuntime::HighLevel::Context &ctx,
       LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    // sanity - make sure that all launch domains are the same size
    assert(x.lDom().get_volume() == y.lDom().get_volume() &&
           y.lDom().get_volume() == w.lDom().get_volume());

    bool xySame = Vector::same(x, y);
    bool xwSame = Vector::same(x, w);
    bool ywSame = Vector::same(y, w);

    ArgumentMap argMap;
    waxpbyTaskArgs taskArgs(alpha, beta, xySame, xwSame);
    IndexLauncher il(LGNCG_WAXPBY_TID, x.lDom(),
                     TaskArgument(&taskArgs, sizeof(taskArgs)), argMap);
    // x's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        RegionRequirement(x.lp(), 0, xwSame ? READ_WRITE : READ_ONLY,
                          EXCLUSIVE, x.lr)
    );
    il.add_field(idx++, x.fid);
    // y's regions /////////////////////////////////////////////////////////////
    if (!xySame) {
        il.add_region_requirement(
            RegionRequirement(y.lp(), 0, ywSame ? READ_WRITE : READ_ONLY,
                              EXCLUSIVE, y.lr)
        );
        il.add_field(idx++, y.fid);
    }
    // w's regions /////////////////////////////////////////////////////////////
    if (!xwSame && !ywSame) {
        il.add_region_requirement(
            RegionRequirement(w.lp(), 0, WRITE_DISCARD, EXCLUSIVE, w.lr)
        );
        il.add_field(idx++, w.fid);
    }
    // execute the thing...
    (void)lrt->execute_index_space(ctx, il);
}

/**
 * computes the update of a vector with the sum of two scaled vectors
 * where: w = alpha * x + beta * y
 */
static inline uint64_t __attribute__((always_inline))
rdtsc (void) 
{
    uint32_t lo, hi;
    asm volatile(//"cpuid\n\t"
                 "rdtsc" : 
                 "=a" (lo), "=d" (hi));
    return lo | ((uint64_t)hi << 32);
}
inline void
waxpbyTask(const LegionRuntime::HighLevel::Task *task,
           const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
           LegionRuntime::HighLevel::Context ctx,
           LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
	
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx; (void)lrt;
    const waxpbyTaskArgs targs = *(waxpbyTaskArgs *)task->args;
    uint8_t xRID = 0;
    uint8_t yRID = 1;
    uint8_t wRID = 2;
    // aliased regions
    if (2 == rgns.size()) {
        bool xySame = targs.xySame;
        bool xwSame = targs.xwSame;
        yRID = xySame ? 0 : 1;
        wRID = xwSame ? 0 : 1;
    }
    else {
        assert(3 == rgns.size());
    }
    // name the regions
    const PhysicalRegion &xpr = rgns[xRID];
    const PhysicalRegion &ypr = rgns[yRID];
    const PhysicalRegion &wpr = rgns[wRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    // vectors
    GDRA x = xpr.get_field_accessor(0).typeify<double>();
    const Domain xDom = lrt->get_index_space_domain(
        ctx, task->regions[xRID].region.get_index_space()
    );
    GDRA y = ypr.get_field_accessor(0).typeify<double>();
    GDRA w = wpr.get_field_accessor(0).typeify<double>();
    // now, actually perform the computation
    const double alpha = targs.alpha;
    const double beta  = targs.beta;
    if (denseWAXPBY(xDom.get_rect<1>(), alpha, beta, x, y, w)) return;
    // else slow path
    for (GenericPointInRectIterator<1> itr(xDom.get_rect<1>()); itr; itr++) {
        double tmp = (alpha * x.read(DomainPoint::from_point<1>(itr.p))) +
                     (beta  * y.read(DomainPoint::from_point<1>(itr.p)));
        w.write(DomainPoint::from_point<1>(itr.p), tmp);
    }

}

} // end lgncg namespace

#endif

