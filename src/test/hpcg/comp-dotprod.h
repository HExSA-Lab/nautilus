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

#ifndef LGNCG_COMP_DOTPROD_H_INCLUDED
#define LGNCG_COMP_DOTPROD_H_INCLUDED

#include "tids.h"
#include "vector.h"
#include "utils.h"
#include "dotprod-accumulate.h"

#include "legion.h"

namespace {

#define LGNCG_DENSE_DOTPROD_STRIP_SIZE 256

#define GDRA LegionRuntime::Accessor::RegionAccessor \
    <LegionRuntime::Accessor::AccessorType::Generic, double>

/**
 * Adapted from another Legion CG code.
 */
bool
denseDotProd(const Rect<1> &subGridBounds,
             double &result,
             GDRA &faX,
             GDRA &faY)
{
    using namespace lgncg;
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;

	unsigned long long start = rdtsc();

    Rect<1> subrect;
    ByteOffset inOffsets[1], offsets[1];
    //
    const double *xp = faX.raw_rect_ptr<1>(
        subGridBounds, subrect, inOffsets
    );
    if (!xp || (subrect != subGridBounds) ||
        !offsetsAreDense<1, double>(subGridBounds, inOffsets)) return false;
    //
    double *yp = faY.raw_rect_ptr<1>(
        subGridBounds, subrect, offsets
    );
    if (!yp || (subrect != subGridBounds) ||
        offsetMismatch(1, inOffsets, offsets)) return false;
    // if we are here, then let the calculation begin
    int nPnts = subGridBounds.volume();
    result = 0.0;
    while (nPnts > 0) {
        if (nPnts >= LGNCG_DENSE_DOTPROD_STRIP_SIZE) {
            for (int i = 0; i < LGNCG_DENSE_DOTPROD_STRIP_SIZE; i++) {
                result += (xp[i] * yp[i]);
            }
            nPnts -= LGNCG_DENSE_DOTPROD_STRIP_SIZE;
            xp += LGNCG_DENSE_DOTPROD_STRIP_SIZE;
            yp += LGNCG_DENSE_DOTPROD_STRIP_SIZE;
        }
        else {
            for (int i = 0; i < nPnts; i++) {
                result += (xp[i] * yp[i]);
            }
            nPnts = 0;
        }
    }
	unsigned long long stop = rdtsc();
	//printf("dense dot prod took %llu cycles\n", stop-start);
    return true;
}

#undef GDRA

}

namespace lgncg {

/**
 * responsible for setting up the task launch of the dot product task.
 */
static inline void
dotprod(Vector &x,
        Vector &y,
        double &result,
        LegionRuntime::HighLevel::Context &ctx,
        LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
	
	unsigned long long start = rdtsc();
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    // sanity - make sure that all launch domains are the same size
    assert(x.lDom().get_volume() == y.lDom().get_volume());
    // no per-task arguments. CGTaskArgs has task-specific info.
    ArgumentMap argMap;
    IndexLauncher il(LGNCG_DOTPROD_TID, x.lDom(),
                     TaskArgument(NULL, 0), argMap);
    // x's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        RegionRequirement(x.lp(), 0, READ_ONLY, EXCLUSIVE, x.lr)
    );
    il.add_field(idx++, x.fid);
    // y's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        RegionRequirement(y.lp(), 0, READ_ONLY, EXCLUSIVE, y.lr)
    );
    il.add_field(idx++, y.fid);

	unsigned long long stop = rdtsc();

	//printf("dotprod: first part = %llu cycles\n", stop-start);

	start = rdtsc();
    // execute the thing...
    Future fResult = lrt->execute_index_space(ctx, il, LGNCG_DOTPROD_RED_ID);
    // now sum up all the answers
    result = fResult.get_result<double>();
	stop = rdtsc();
	//printf("dotprod: second part = %llu cycles\n", stop-start);
}

/**
 * computes the dot product of x' and y.
 * where: result = x' * y
 */
double
dotProdTask(const LegionRuntime::HighLevel::Task *task,
            const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
            LegionRuntime::HighLevel::Context ctx,
            LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
	unsigned long long start = rdtsc();
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx; (void)lrt;
    static const uint8_t xRID = 0;
    static const uint8_t yRID = 1;
    // x, y
    assert(2 == rgns.size());
#if 0 // nice debug
    printf("%d: sub-grid bounds: (%d) to (%d)\n",
            getTaskID(task), rect.lo.x[0], rect.hi.x[0]);
#endif
    // name the regions
    const PhysicalRegion &xpr = rgns[xRID];
    const PhysicalRegion &ypr = rgns[yRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    // vectors
    GDRA x = xpr.get_field_accessor(0).typeify<double>();
    const Domain xDom = lrt->get_index_space_domain(
        ctx, task->regions[xRID].region.get_index_space()
    );
    GDRA y = ypr.get_field_accessor(0).typeify<double>();
    // now, actually perform the computation
    double localRes = 0.0;
	unsigned long long start2 = rdtsc();
    if (denseDotProd(xDom.get_rect<1>(), localRes, x, y)) {
		unsigned long long stop2 = rdtsc();
		goto out1;
        //return localRes;
    }
    // else slower path
    for (GenericPointInRectIterator<1> itr(xDom.get_rect<1>()); itr; itr++) {
        localRes += x.read(DomainPoint::from_point<1>(itr.p)) *
                    y.read(DomainPoint::from_point<1>(itr.p));
    }

out1:
	unsigned long long stop = rdtsc();
	//printf("DotProdTask took %llu cycles\n", stop-start);
    return localRes;
}

} // end lgncg namespace

#endif
