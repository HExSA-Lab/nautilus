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

#ifndef LGNCG_COMP_SYMGS_H_INCLUDED
#define LGNCG_COMP_SYMGS_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "tids.h"

#include "legion.h"

namespace {

struct symgsTaskArgs {
    uint8_t sweepi;
    uint64_t nMatCols;

    symgsTaskArgs(uint8_t s, uint64_t n) : sweepi(s), nMatCols(n) { ; }
};

}

/**
 * implements one step of SYMmetric Gauss-Seidel.
 */
namespace lgncg {

/**
 * responsible for setting up the task launch of the symmetric gauss-seidel
 * where x is unknown.
 */
static inline void
symgs(const SparseMatrix &A,
      Vector &x,
      const Vector &r,
      LegionRuntime::HighLevel::Context &ctx,
      LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    // sanity - make sure that all launch domains are the same size
    assert(A.vals.lDom().get_volume() == x.lDom().get_volume() &&
           x.lDom().get_volume() == r.lDom().get_volume());
    // create tmp array - not ideal, but seems to work.
    Vector tmpX;
    tmpX.create<double>(x.len, ctx, lrt);
    tmpX.partition(A.nParts, ctx, lrt);
    ArgumentMap argMap;
    // FIXME - i don't like this. Performance and memory usage shortcomings.
    static const uint8_t N_SWEEPS = 2;
    for (uint8_t sweepi = 0; sweepi < N_SWEEPS; ++sweepi) {
        veccp(x, tmpX, ctx, lrt);
        symgsTaskArgs taskArgs(sweepi, A.nCols);
        int idx = 0;
        IndexLauncher il(LGNCG_SYMGS_TID, A.vals.lDom(),
                         TaskArgument(&taskArgs, sizeof(taskArgs)), argMap);
        // A's regions /////////////////////////////////////////////////////////
        // vals
        il.add_region_requirement(
            RegionRequirement(A.vals.lp(), 0, READ_ONLY, EXCLUSIVE, A.vals.lr)
        );
        il.add_field(idx++, A.vals.fid);
        // diag
        il.add_region_requirement(
            RegionRequirement(A.diag.lp(), 0, READ_ONLY, EXCLUSIVE, A.diag.lr)
        );
        il.add_field(idx++, A.diag.fid);
        // mIdxs
        il.add_region_requirement(
            RegionRequirement(A.mIdxs.lp(), 0, READ_ONLY, EXCLUSIVE, A.mIdxs.lr)
        );
        il.add_field(idx++, A.mIdxs.fid);
        // nzir
        il.add_region_requirement(
            RegionRequirement(A.nzir.lp(), 0, READ_ONLY, EXCLUSIVE, A.nzir.lr)
        );
        il.add_field(idx++, A.nzir.fid);
        // x's regions /////////////////////////////////////////////////////////
        // read/write view of x
        il.add_region_requirement(
            RegionRequirement(tmpX.lp(), 0, READ_WRITE, EXCLUSIVE, tmpX.lr)
        );
        il.add_field(idx++, tmpX.fid);
        // read only view of all of x. FIXME only use required cells
        il.add_region_requirement(
            /* notice we are using the entire region here */
            RegionRequirement(x.lr, 0, READ_ONLY, EXCLUSIVE, x.lr)
        );
        il.add_field(idx++, x.fid);
        // r's regions /////////////////////////////////////////////////////////
        il.add_region_requirement(
            RegionRequirement(r.lp(), 0, READ_ONLY, EXCLUSIVE, r.lr)
        );
        il.add_field(idx++, r.fid);
        // execute the thing...
        (void)lrt->execute_index_space(ctx, il);
        // copy back
        veccp(tmpX, x, ctx, lrt);
    }
    tmpX.free(ctx, lrt);
}

/**
 * computes: symgs for Ax = r
 */
inline void
symgsTask(const LegionRuntime::HighLevel::Task *task,
          const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
          LegionRuntime::HighLevel::Context ctx,
          LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx; (void)lrt;
    static const uint8_t aValsRID  = 0;
    static const uint8_t aDiagRID  = 1;
    static const uint8_t aMIdxsRID = 2;
    static const uint8_t aNZiRRID  = 3;
    static const uint8_t xRWRID    = 4;
    static const uint8_t xRID      = 5;
    static const uint8_t rRID      = 6;
    // A (x4), x (x2), b
    assert(7 == rgns.size());
    const symgsTaskArgs args = *(symgsTaskArgs *)task->args;
    const int64_t nMatCols = args.nMatCols;
    // name the regions
    // spare matrix regions
    const PhysicalRegion &avpr = rgns[aValsRID];
    const PhysicalRegion &adpr = rgns[aDiagRID];
    const PhysicalRegion &aipr = rgns[aMIdxsRID];
    const PhysicalRegion &azpr = rgns[aNZiRRID];
    // vector regions
    const PhysicalRegion &xrwpr = rgns[xRWRID]; // read/write sub-region
    const PhysicalRegion &xpr   = rgns[xRID]; // read only region (entire)
    const PhysicalRegion &rpr   = rgns[rRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>  GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t> GLRA;
    typedef RegionAccessor<AccessorType::Generic, uint8_t> GSRA;
    // sparse matrix
    GDRA av = avpr.get_field_accessor(0).typeify<double>();
    const Domain aValsDom = lrt->get_index_space_domain(
        ctx, task->regions[aValsRID].region.get_index_space()
    );
    GDRA ad = adpr.get_field_accessor(0).typeify<double>();
    const Domain aDiagDom = lrt->get_index_space_domain(
        ctx, task->regions[aDiagRID].region.get_index_space()
    );
    GLRA ai = aipr.get_field_accessor(0).typeify<int64_t>();
    GSRA az = azpr.get_field_accessor(0).typeify<uint8_t>();
    // vectors
    GDRA xrw = xrwpr.get_field_accessor(0).typeify<double>();
    const Domain xRWDom = lrt->get_index_space_domain(
        ctx, task->regions[xRWRID].region.get_index_space()
    );
    GDRA x = xpr.get_field_accessor(0).typeify<double>();
    const Domain xDom = lrt->get_index_space_domain(
        ctx, task->regions[xRID].region.get_index_space()
    );
    GDRA r = rpr.get_field_accessor(0).typeify<double>();
    const Domain rDom = lrt->get_index_space_domain(
        ctx, task->regions[rRID].region.get_index_space()
    );
    // calculate nRows and nCols for the local subgrid
    Rect<1> myGridBounds = aValsDom.get_rect<1>();
    assert(0 == myGridBounds.volume() % nMatCols);
    const int64_t lNRows = myGridBounds.volume() / nMatCols;
    const int64_t lNCols = nMatCols;
    //
    Rect<1> avsr; ByteOffset avOff[1];
    const double *const avp = av.raw_rect_ptr<1>(myGridBounds, avsr, avOff);
    bool offd = offsetsAreDense<1, double>(myGridBounds, avOff);
    assert(offd);
    // remember that vals and mIdxs should be the same size
    Rect<1> aisr; ByteOffset aiOff[1];
    const int64_t *const aip = ai.raw_rect_ptr<1>(myGridBounds, aisr, aiOff);
    offd = offsetsAreDense<1, int64_t>(myGridBounds, aiOff);
    assert(offd);
    // diag and nzir are smaller (by a stencil size factor).
    Rect<1> adsr; ByteOffset adOff[1];
    myGridBounds = aDiagDom.get_rect<1>();
    const double *const adp = ad.raw_rect_ptr<1>(myGridBounds, adsr, adOff);
    offd = offsetsAreDense<1, double>(myGridBounds, adOff);
    assert(offd);
    // remember nzir and diag are the same length
    Rect<1> azsr; ByteOffset azOff[1];
    const uint8_t *const azp = az.raw_rect_ptr<1>(myGridBounds, azsr, azOff);
    offd = offsetsAreDense<1, uint8_t>(myGridBounds, adOff);
    assert(offd);
    // x - read/write
    myGridBounds = xRWDom.get_rect<1>();
    Rect<1> xrwsr; ByteOffset xrwOff[1];
    double *xrwp = xrw.raw_rect_ptr<1>(myGridBounds, xrwsr, xrwOff);
    offd = offsetsAreDense<1, double>(myGridBounds, xrwOff);
    assert(offd);
    // x (all of X)
    Rect<1> xsr; ByteOffset xOff[1];
    myGridBounds = xDom.get_rect<1>();
    const double *const xp = x.raw_rect_ptr<1>(myGridBounds, xsr, xOff);
    offd = offsetsAreDense<1, double>(myGridBounds, xOff);
    assert(offd);
    // r
    Rect<1> rsr; ByteOffset rOff[1];
    myGridBounds = rDom.get_rect<1>();
    const double *const rp = r.raw_rect_ptr<1>(myGridBounds, rsr, rOff);
    offd = offsetsAreDense<1, double>(myGridBounds, rOff);
    assert(offd);
    // now, actually perform the computation
    // forward sweep
    if (0 == args.sweepi) {
        for (int64_t i = 0; i < lNRows; ++i) {
            // get to base of next row of values
            const double *const cVals = (avp + (i * lNCols));
            // get to base of next row of "real" indices of values
            const int64_t *const cIndx = (aip + (i * lNCols));
            // capture how many non-zero values are in this particular row
            const int64_t cnnz = azp[i];
            // current diagonal value
            const double curDiag = adp[i];
            // RHS value
            double sum = rp[i];
            for (int64_t j = 0; j < cnnz; ++j) {
                const int64_t curCol = cIndx[j];
                sum -= cVals[j] * xp[curCol];
            }
            // remove diagonal contribution from previous loop
            sum += xrwp[i] * curDiag;
            xrwp[i] = sum / curDiag;
        }
    }
    else {
        // back sweep
        for (int64_t i = lNRows - 1; i >= 0; --i) {
            // get to base of next row of values
            const double *const cVals = (avp + (i * lNCols));
            // get to base of next row of "real" indices of values
            const int64_t *const cIndx = (aip + (i * lNCols));
            // capture how many non-zero values are in this particular row
            const int64_t cnnz = azp[i];
            // current diagonal value
            const double curDiag = adp[i];
            // RHS value
            double sum = rp[i]; // RHS value
            for (int64_t j = 0; j < cnnz; ++j) {
                const int64_t curCol = cIndx[j];
                sum -= cVals[j] * xp[curCol];
            }
            // remove diagonal contribution from previous loop
            sum += xrwp[i] * curDiag;
            xrwp[i] = sum / curDiag;
        }
    }
}

}

#endif
