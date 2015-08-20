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

#ifndef LGNCG_COMP_SPMV_H_INCLUDED
#define LGNCG_COMP_SPMV_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "tids.h"

#include "legion.h"

namespace {

#define LGNCG_DENSE_SPMV_STRIP_SIZE 256

#define GDRA LegionRuntime::Accessor::RegionAccessor \
    <LegionRuntime::Accessor::AccessorType::Generic, double>

#define GLRA LegionRuntime::Accessor::RegionAccessor \
    <LegionRuntime::Accessor::AccessorType::Generic, int64_t>

struct spmvTaskArgs {
    int64_t nCols;
    spmvTaskArgs(int64_t nCols) : nCols(nCols) { ; }
};

/**
 * Adapted from another Legion CG code.
 */
template<int MAX_NZEROS>
bool
denseSPMV(
    const Rect<1> &subGridBounds,
    const Rect<1> &elemBounds,
    const Rect<1> &vecBounds,
    GLRA &faCol,
    GDRA &faVal,
    GDRA &faX,
    GDRA &faY)
{
    using namespace lgncg;
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;

    Rect<1> subrect;
    ByteOffset inOffsets[1], offsets[1];
    //
    const int64_t *colp = faCol.raw_rect_ptr<1>(
        elemBounds, subrect, inOffsets
    );
    if (!colp || (subrect != elemBounds) ||
        !offsetsAreDense<1, double>(elemBounds, inOffsets)) return false;
    //
    const double *valp = faVal.raw_rect_ptr<1>(
        elemBounds, subrect, offsets
    );
    if (!valp || (subrect != elemBounds) ||
        offsetMismatch(1, inOffsets, offsets)) return false;
    //
    const double *xp = faX.raw_rect_ptr<1>(
        vecBounds, subrect, offsets
    );
    if (!xp || (subrect != vecBounds) ||
        !offsetsAreDense<1, double>(vecBounds, offsets)) return false;
    //
    double *yp = faY.raw_rect_ptr<1>(
        subGridBounds, subrect, offsets
    );
    if (!yp || (subrect != subGridBounds) ||
        !offsetsAreDense<1, double>(subGridBounds, offsets)) return false;
    // if we are here, then let the calculation begin
    int nRows = subGridBounds.volume();
    while (nRows > 0) {
        if (nRows >= LGNCG_DENSE_SPMV_STRIP_SIZE) {
            for (int i = 0; i < LGNCG_DENSE_SPMV_STRIP_SIZE; ++i) {
                double sum = 0.0;
                for (int j = 0; j < MAX_NZEROS; ++j) {
                    const int index = colp[i * MAX_NZEROS + j];
                    const double val = valp[i * MAX_NZEROS + j];
                    const double xval = xp[index];
                    sum += (val * xval);
                }
                yp[i] = sum;
            }
            nRows -= LGNCG_DENSE_SPMV_STRIP_SIZE;
            colp += (MAX_NZEROS * LGNCG_DENSE_SPMV_STRIP_SIZE);
            valp += (MAX_NZEROS * LGNCG_DENSE_SPMV_STRIP_SIZE);
            yp += LGNCG_DENSE_SPMV_STRIP_SIZE;
        }
        else {
            for (int i = 0; i < nRows; ++i) {
                double sum = 0.0;
                for (int j = 0; j < MAX_NZEROS; ++j) {
                    const int index = colp[i * MAX_NZEROS + j];
                    const double val = valp[i * MAX_NZEROS + j];
                    const double xval = xp[index];
                    sum += (val * xval);
                }
                yp[i] = sum;
            }
            nRows = 0;
        }
    }
    return true;
}

#undef GDRA
#undef GLRA

}

/**
 * implements the SParse Matrix Vector multiply functionality.
 */

namespace lgncg {

/**
 * responsible for setting up the task launch of the sparse matrix vector
 * multiply.
 */
static inline void
spmv(const SparseMatrix &A,
     const Vector &x,
     Vector &y,
     LegionRuntime::HighLevel::Context &ctx,
     LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    // sanity - make sure that all launch domains are the same size
    assert(A.vals.lDom().get_volume() == x.lDom().get_volume() &&
           x.lDom().get_volume() == y.lDom().get_volume());
    // setup per-task args
    ArgumentMap argMap;
    spmvTaskArgs targs(A.nCols);
    IndexLauncher il(LGNCG_SPMV_TID, A.vals.lDom(),
                     TaskArgument(&targs, sizeof(targs)), argMap);
    // A's regions /////////////////////////////////////////////////////////////
    // vals
    il.add_region_requirement(
        RegionRequirement(A.vals.lp(), 0, READ_ONLY, EXCLUSIVE, A.vals.lr)
    );
    il.add_field(idx++, A.vals.fid);
    // mIdxs
    il.add_region_requirement(
        RegionRequirement(A.mIdxs.lp(), 0, READ_ONLY, EXCLUSIVE, A.mIdxs.lr)
    );
    il.add_field(idx++, A.mIdxs.fid);
    // x's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        /* notice we are using the entire region here */
        RegionRequirement(x.lr, 0, READ_ONLY, EXCLUSIVE, x.lr)
    );
    il.add_field(idx++, x.fid);
    // y's regions /////////////////////////////////////////////////////////////
    il.add_region_requirement(
        RegionRequirement(y.lp(), 0, WRITE_DISCARD, EXCLUSIVE, y.lr)
    );
    il.add_field(idx++, y.fid);
    // execute the thing...
    (void)lrt->execute_index_space(ctx, il);
}

/**
 * computes: y = A * x ; where y and x are vectors and A is a sparse matrix
 */
inline void
spmvTask(const LegionRuntime::HighLevel::Task *task,
          const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
          LegionRuntime::HighLevel::Context ctx,
          LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx;
    static const uint8_t aValsRID  = 0;
    static const uint8_t aMIdxsRID = 1;
    static const uint8_t xRID      = 2;
    static const uint8_t yRID      = 3;
    // A (x2), x, b
    assert(4 == rgns.size());
    const spmvTaskArgs targs = *(spmvTaskArgs *)task->args;
#if 0 // nice debug
    printf("%d: sub-grid bounds: (%d) to (%d)\n",
            getTaskID(task), rect.lo.x[0], rect.hi.x[0]);
#endif
    // name the regions
    const PhysicalRegion &avpr = rgns[aValsRID];
    const PhysicalRegion &aipr = rgns[aMIdxsRID];
    const PhysicalRegion &xpr  = rgns[xRID];
    const PhysicalRegion &ypr  = rgns[yRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t> GLRA;
    typedef RegionAccessor<AccessorType::Generic, uint8_t> GSRA;
    // sparse matrix
    GDRA av = avpr.get_field_accessor(0).typeify<double>();
    const Domain aValsDom = lrt->get_index_space_domain(
        ctx, task->regions[aValsRID].region.get_index_space()
    );
    GLRA ai = aipr.get_field_accessor(0).typeify<int64_t>();
    const Domain aMIdxsDom = lrt->get_index_space_domain(
        ctx, task->regions[aMIdxsRID].region.get_index_space()
    );
    // vectors
    GDRA x = xpr.get_field_accessor(0).typeify<double>();
    const Domain xDom = lrt->get_index_space_domain(
        ctx, task->regions[xRID].region.get_index_space()
    );
    GDRA y = ypr.get_field_accessor(0).typeify<double>();
    const Domain yDom = lrt->get_index_space_domain(
        ctx, task->regions[yRID].region.get_index_space()
    );
    // now, actually perform the computation
    const int64_t maxNon0sInCol = targs.nCols;
    const int64_t lNRows = aValsDom.get_rect<1>().volume() / maxNon0sInCol;
    Rect<1> rowRect  = yDom.get_rect<1>();
    Rect<1> elemRect = aMIdxsDom.get_rect<1>();
    Rect<1> vecRect  = xDom.get_rect<1>();
    // try fast path
    switch (maxNon0sInCol) {
        case 27: {
            if (denseSPMV<27>(rowRect, elemRect, vecRect, ai, av, x, y)) {
                // done
                return;
            }
        }
    }
    // if here, then we are going with the slower path
    DomainPoint pir; pir.dim = 1;
    GenericPointInRectIterator<1> rowItr(yDom.get_rect<1>());
    GenericPointInRectIterator<1> ciItr(aMIdxsDom.get_rect<1>());
    for (int64_t i = 0; i < lNRows; ++i, rowItr++) {
        double sum = 0.0;
        for (int64_t j = 0; j < maxNon0sInCol; ++j, ciItr++) {
            int64_t index = ai.read(DomainPoint::from_point<1>(ciItr.p));
            pir.point_data[0] = index;
            sum += av.read(DomainPoint::from_point<1>(ciItr.p)) * x.read(pir);
        }
        y.write(DomainPoint::from_point<1>(rowItr.p), sum);
    }
}

}

#endif
