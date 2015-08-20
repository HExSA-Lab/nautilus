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

#include "hpcg-problem.h"
#include "hpcg-problem-generator.h"

#include "lgncg.h"

int Problem::genProbTID       = -1;
int Problem::populatef2cTID   = -1;

// again - ideally, we just call into some central place to get TIDs.
// pretend this isn't here...
enum {
    GEN_PROB_TID = 2,
    POP_F2C_FID
};

namespace {
/**
 * task arguments for HPCG driver
 */
struct HPCGTaskArgs {
    lgncg::Geometry geom;
    int64_t maxColNonZeros;

    HPCGTaskArgs(const lgncg::Geometry &geom,
                 int64_t maxColNonZeros) :
        geom(geom), maxColNonZeros(maxColNonZeros) { ; }
};

/**
 * task arguments for populatef2cTasks
 */
struct PF2CTaskArgs {
    // fine geometry
    lgncg::Geometry fGeom;
    // coarse geometry
    lgncg::Geometry cGeom;

    PF2CTaskArgs(const lgncg::Geometry &fGeom,
                 const lgncg::Geometry cGeom)
        : fGeom(fGeom), cGeom(cGeom) { ; }
};

} // end namespace

/**
 * wrapper to launch setICsTask. always fill from left to right. A -> x -> y.
 */
void
Problem::setICs(lgncg::SparseMatrix &A,
                lgncg::Vector *x,
                lgncg::Vector *b,
                LegionRuntime::HighLevel::Context &ctx,
                LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using LegionRuntime::Arrays::Rect;

    int idx = 0;
    // setup task args
    HPCGTaskArgs targs(A.geom, A.nCols);
    ArgumentMap argMap;
    // setup task launcher
    IndexLauncher il(Problem::genProbTID, A.vals.lDom(),
                     TaskArgument(&targs, sizeof(targs)), argMap);
    // for each logical region
    //     add a region requirement
    //     and for each field the region contains
    //         add it to the launcher
    // WRITE_DISCARD is a special form of READ_WRITE that still permits
    // the task to perform any kind of operation, but informs the
    // runtime that the task intends to overwrite all previous data
    // stored in the logical region without reading it.  init launcher
    // A's vals
    il.add_region_requirement(
        RegionRequirement(A.vals.lp(), 0, WRITE_DISCARD, EXCLUSIVE, A.vals.lr)
    );
    il.add_field(idx++, A.vals.fid);
    // A's diag values
    il.add_region_requirement(
        RegionRequirement(A.diag.lp(), 0, WRITE_DISCARD, EXCLUSIVE, A.diag.lr)
    );
    il.add_field(idx++, A.diag.fid);
    // A's mIdxs
    il.add_region_requirement(
        RegionRequirement(A.mIdxs.lp(), 0, WRITE_DISCARD, EXCLUSIVE, A.mIdxs.lr)
    );
    il.add_field(idx++, A.mIdxs.fid);
    // A's # non 0s in row
    il.add_region_requirement(
        RegionRequirement(A.nzir.lp(), 0, WRITE_DISCARD, EXCLUSIVE, A.nzir.lr)
    );
    il.add_field(idx++, A.nzir.fid);
    // A's global to global table
    il.add_region_requirement(
        RegionRequirement(A.g2g.lp(), 0, WRITE_DISCARD, EXCLUSIVE, A.g2g.lr)
    );
    il.add_field(idx++, A.g2g.fid);
    // if we are provided a pointer to an x, add it
    if (x) {
        il.add_region_requirement(
            RegionRequirement(x->lp(), 0, WRITE_DISCARD, EXCLUSIVE, x->lr)
        );
        il.add_field(idx++, x->fid);
    }
    // similarly for b
    if (b) {
        il.add_region_requirement(
            RegionRequirement(b->lp(), 0, WRITE_DISCARD, EXCLUSIVE, b->lr)
        );
        il.add_field(idx++, b->fid);
    }
    // ... and go! Wait here for more accurate timings (at least we hope)... The
    // idea is that we want to separate initialization from the solve.
    lrt->execute_index_space(ctx, il).wait_all_results();
}

/**
 * akin to HPCG's GenerateProblem.
 */
void
setICsTask(
    const LegionRuntime::HighLevel::Task *task,
    const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
    LegionRuntime::HighLevel::Context ctx,
    LegionRuntime::HighLevel::HighLevelRuntime *lrt
)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using namespace lgncg;
    (void)ctx;
    static const uint8_t aValsRID  = 0;
    static const uint8_t aDiagRID  = 1;
    static const uint8_t aMIdxsRID = 2;
    static const uint8_t aNZiRRID  = 3;
    static const uint8_t aG2GRID   = 4;
    static const uint8_t xRID      = 5;
    static const uint8_t bRID      = 6;
    // stash my task ID
    const int taskID = task->index_point.point_data[0];
    // capture how many regions were passed to us. this number dictates what
    // regions we will be working on. region order here matters.
    const size_t nRgns = rgns.size();
    // we always have 5 regions for the sparse matrix for this task
    assert(nRgns >= 5 && nRgns <= 7);
    const bool haveX = nRgns > 5;
    const bool haveB = nRgns > 6;
    // get our task arguments
    const HPCGTaskArgs targs = *(HPCGTaskArgs *)task->args;
    ////////////////////////////////////////////////////////////////////////////
    // Sparse Matrix A
    ////////////////////////////////////////////////////////////////////////////
    const PhysicalRegion &avpr  = rgns[aValsRID];
    const PhysicalRegion &adpr  = rgns[aDiagRID];
    const PhysicalRegion &aipr  = rgns[aMIdxsRID];
    const PhysicalRegion &azpr  = rgns[aNZiRRID];
    const PhysicalRegion &g2gpr = rgns[aG2GRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, double>   GDRA;
    typedef RegionAccessor<AccessorType::Generic, int64_t>  GLRA;
    typedef RegionAccessor<AccessorType::Generic, uint8_t>  GSRA;
    typedef RegionAccessor<AccessorType::Generic, I64Tuple> GTRA;
    // get handles to all the matrix accessors that we need
    GDRA av = avpr.get_field_accessor(0).typeify<double>();
    const Domain aValsDom = lrt->get_index_space_domain(
        ctx, task->regions[aValsRID].region.get_index_space()
    );
    Rect<1> aValsRect = aValsDom.get_rect<1>();
    //
    GDRA ad = adpr.get_field_accessor(0).typeify<double>();
    const Domain aDiagDom = lrt->get_index_space_domain(
        ctx, task->regions[aDiagRID].region.get_index_space()
    );
    Rect<1> aDiagRect = aDiagDom.get_rect<1>();
    //
    GLRA ai = aipr.get_field_accessor(0).typeify<int64_t>();
    GSRA az = azpr.get_field_accessor(0).typeify<uint8_t>();
    GTRA at = g2gpr.get_field_accessor(0).typeify<I64Tuple>();
    const Domain aG2GDom = lrt->get_index_space_domain(
        ctx, task->regions[aG2GRID].region.get_index_space()
    );
    Rect<1> aG2GRect = aG2GDom.get_rect<1>();
    ////////////////////////////////////////////////////////////////////////////
    // all problem setup logic in the ProblemGenerator /////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // construct new initial conditions for this sub-region
    ProblemGenerator ic(targs.geom, targs.maxColNonZeros, taskID);
    // setup to avoid memory bloat during init
    // the bounds of all entries
    typedef GenericPointInRectIterator<1> GPRI1D;
    typedef DomainPoint DomPt;
    do {
        const int64_t nLocalCols = targs.maxColNonZeros;
        const int64_t nLocalRows = aDiagRect.volume();
        GPRI1D p(aValsRect);
        GPRI1D q(aDiagRect);
        for (int64_t i = 0; i < nLocalRows; ++i, q++) {
            for (int64_t j = 0; j < nLocalCols; ++j, p++) {
                av.write(DomPt::from_point<1>(p.p), ic.A[i][j]);
                ai.write(DomPt::from_point<1>(p.p), ic.mtxInd[i][j]);
            }
            // SKG - i'm a little confused by why matDiag is not just an array
            ad.write(DomPt::from_point<1>(q.p), ic.matDiag[i][0]);
            az.write(DomPt::from_point<1>(q.p), ic.non0sInRow[i]);
        }
    } while(0);
    // Populate g2g
    do {
        const int64_t offset = taskID * aG2GRect.volume();
        int64_t row = 0;
        for (GPRI1D p(aG2GRect); p; p++, row++) {
            at.write(DomPt::from_point<1>(p.p),
                     I64Tuple(offset + row, ic.l2gTab[row]));
        }
    } while(0);
    if (haveX) {
        ////////////////////////////////////////////////////////////////////////
        // Vector x - initial guess of all zeros
        ////////////////////////////////////////////////////////////////////////
        const PhysicalRegion &vecXPr = rgns[xRID];
        GDRA x = vecXPr.get_field_accessor(0).typeify<double>();
        const Domain xDom = lrt->get_index_space_domain(
            ctx, task->regions[xRID].region.get_index_space()
        );
        Rect<1> xRect = xDom.get_rect<1>();
        for (GPRI1D p(xRect); p; p++) {
            x.write(DomPt::from_point<1>(p.p), 0.0);
        }
    }
    if (haveB) {
        ////////////////////////////////////////////////////////////////////////
        // Vector b
        ////////////////////////////////////////////////////////////////////////
        const PhysicalRegion &vecBPr = rgns[bRID];
        GDRA b = vecBPr.get_field_accessor(0).typeify<double>();
        const Domain bDom = lrt->get_index_space_domain(
            ctx, task->regions[bRID].region.get_index_space()
        );
        Rect<1> bRect = bDom.get_rect<1>();
        GPRI1D p(bRect);
        for (int64_t i = 0; p; ++i, p++) {
            b.write(DomPt::from_point<1>(p.p), ic.b[i]);
        }
    }
}

/**
 * wrapper to launch genCoarseProblem.
 */
void
Problem::genCoarseProbGeom(lgncg::SparseMatrix &Af,
                           LegionRuntime::HighLevel::Context &ctx,
                           LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using namespace lgncg;
    using lgncg::Geometry;

    const int64_t npx = Af.geom.npx;
    const int64_t npy = Af.geom.npy;
    const int64_t npz = Af.geom.npz;
    const int64_t nx  = Af.geom.nx;
    const int64_t ny  = Af.geom.ny;
    const int64_t nz  = Af.geom.nz;
    const int64_t cnx = nx / 2;
    const int64_t cny = ny / 2;
    const int64_t cnz = nz / 2;
    const int64_t globalXYZ =  (npx * nx)  * (npy * ny)  * (npz * nz);
    const int64_t cGlobalXYZ = (npx * cnx) * (npy * cny) * (npz * cnz);
    // sanity
    assert(0 == (nx % 2) && 0 == (ny % 2) && 0 == (nz % 2));
    assert(Af.nRows == globalXYZ);
    // now construct the required data structures for the coarse grid
    Af.Ac = new lgncg::SparseMatrix();
    assert(Af.Ac);
    // set the coarse geometry
    const Geometry coarseGeom = Geometry(Af.geom.size,
                                         npx, npy, npz,
                                         cnx, cny, cnz);
    // now create the coarse grid matrix
    Af.Ac->create(coarseGeom, cGlobalXYZ, Af.nCols, ctx, lrt);
    Af.Ac->partition(Af.geom, ctx, lrt);
    Af.mgData = new lgncg::MGData(globalXYZ, cGlobalXYZ, ctx, lrt);
    assert(Af.mgData);
    Af.mgData->partition(Af.nParts, ctx, lrt);
    // populate the fine to coarse operator
    populatef2c(Af, Af.geom, Af.Ac->geom, ctx, lrt);
    // set initial conditions at this level
    setICs(*Af.Ac, NULL, NULL, ctx, lrt);
    // now setup the halo the current level
    setupHalo(*Af.Ac, ctx, lrt);
}

/**
 * wrapper to launch populatef2cTasks
 */
void
Problem::populatef2c(lgncg::SparseMatrix &Af,
                     const lgncg::Geometry &fineGeom,
                     lgncg::Geometry &coarseGeom,
                     LegionRuntime::HighLevel::Context &ctx,
                     LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using LegionRuntime::Arrays::Rect;
    // start task launch prep
    int idx = 0;
    ArgumentMap argMap;
    PF2CTaskArgs targs(fineGeom, coarseGeom);
    lgncg::MGData *mgd = Af.mgData;
    IndexLauncher il(Problem::populatef2cTID, mgd->f2cOp.lDom(),
                     TaskArgument(&targs, sizeof(targs)), argMap);
    // f2cOp
    il.add_region_requirement(
        RegionRequirement(mgd->f2cOp.lp(), 0, WRITE_DISCARD,
                          EXCLUSIVE, mgd->f2cOp.lr)
    );
    il.add_field(idx++, mgd->f2cOp.fid);
    // populate the fine to coarse operator vector
    (void)lrt->execute_index_space(ctx, il);
}

/**
 * this task is responsible for populating the fine to coarse operator
 */
void
populatef2cTask(
    const LegionRuntime::HighLevel::Task *task,
    const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
    LegionRuntime::HighLevel::Context ctx,
    LegionRuntime::HighLevel::HighLevelRuntime *lrt
)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using namespace lgncg;
    (void)ctx;
    (void)lrt;
    static const uint8_t f2cOpFID = 0;
    // f2cOp
    assert(1 == rgns.size());
    const PF2CTaskArgs args = *(PF2CTaskArgs *)task->args;
    const Geometry &fGeom = args.fGeom;
    const Geometry &cGeom = args.cGeom;
    // name the regions
    const PhysicalRegion &f2cpr = rgns[f2cOpFID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, int64_t>  GIRA;
    GIRA f2c = f2cpr.get_field_accessor(0).typeify<int64_t>();
    const Domain f2cOpDom = lrt->get_index_space_domain(
        ctx, task->regions[f2cOpFID].region.get_index_space()
    );
    const int64_t nxf = fGeom.nx;
    const int64_t nyf = fGeom.ny;
    const int64_t nxc = cGeom.nx;
    const int64_t nyc = cGeom.ny;
    const int64_t nzc = cGeom.nz;
    GenericPointInRectIterator<1> f2cOpItr(f2cOpDom.get_rect<1>());
    for (int64_t izc = 0; izc < nzc; ++izc) {
        const int64_t izf = 2 * izc;
        for (int64_t iyc = 0; iyc < nyc; ++iyc) {
            const int64_t iyf = 2 * iyc;
            for (int64_t ixc = 0; ixc < nxc; ++ixc) {
                const int64_t ixf = 2 * ixc;
                //const int64_t currentCoarseRow = izc * nxc * nyc + iyc * nxc + ixc;
                const int64_t currentFineRow   = izf * nxf * nyf + iyf * nxf + ixf;
                f2c.write(DomainPoint::from_point<1>(f2cOpItr.p), currentFineRow);
                f2cOpItr++;
                //f2cp[currentCoarseRow] = currentFineRow;
            } // end iy loop
        } // end even iz if statement
    } // end iz loop
}

/**
 * registers any legion tasks that Problem may use.
 */
void
Problem::init(void)
{
    using namespace LegionRuntime::HighLevel;
    // register any tasks here -- exactly once
    static bool registeredTasks = false;
    if (!registeredTasks) {
        Problem::genProbTID = GEN_PROB_TID;
        // register the generate problem task
        HighLevelRuntime::register_legion_task<setICsTask>(
            Problem::genProbTID /* task id */,
            Processor::LOC_PROC /* proc kind  */,
            true /* single */,
            true /* index */,
            AUTO_GENERATE_ID,
            TaskConfigOptions(true /* leaf task */),
            "set-ics-task"
        );
        // register the populate f2cOperator task
        Problem::populatef2cTID = POP_F2C_FID;
        HighLevelRuntime::register_legion_task<populatef2cTask>(
            Problem::populatef2cTID /* task id */,
            Processor::LOC_PROC /* proc kind  */,
            true /* single */,
            true /* index */,
            AUTO_GENERATE_ID,
            TaskConfigOptions(true /* leaf task */),
            "populate-f2c-task"
        );
        registeredTasks = true;
    }
}

