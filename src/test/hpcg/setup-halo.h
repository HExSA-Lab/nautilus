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
 *
 * -- High Performance Conjugate Gradient Benchmark (HPCG)
 *    HPCG - 2.1 - January 31, 2014
 *
 *    Michael A. Heroux
 *    Scalable Algorithms Group, Computing Research Center
 *    Sandia National Laboratories, Albuquerque, NM
 *
 *    Piotr Luszczek
 *    Jack Dongarra
 *    University of Tennessee, Knoxville
 *    Innovative Computing Laboratory
 *    (C) Copyright 2013 All Rights Reserved
 *
 */

#ifndef LGNCG_SETUP_HALO_H_INCLUDED
#define LGNCG_SETUP_HALO_H_INCLUDED

#include "vector.h"
#include "sparsemat.h"
#include "utils.h"
#include "tids.h"

#include "../../legion_runtime/legion.h"

#include <map>

/**
 * Implements the halo setup. Well... Not really. Just updates indices for now.
 * This will implement halo setup at some point.
 */

namespace lgncg {

/**
 *
 */
static inline void
setupHalo(SparseMatrix &A,
          LegionRuntime::HighLevel::Context &ctx,
          LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    int idx = 0;
    ArgumentMap argMap;
    IndexLauncher il(LGNCG_SETUP_HALO_TID, A.nzir.lDom(),
                     TaskArgument(NULL, 0), argMap);
    // A's regions /////////////////////////////////////////////////////////////
    // mIdxs
    il.add_region_requirement(
        RegionRequirement(A.mIdxs.lp(), 0, READ_WRITE, EXCLUSIVE, A.mIdxs.lr)
    );
    il.add_field(idx++, A.mIdxs.fid);
    // global to global row lookup table (all of it)
    il.add_region_requirement(
        RegionRequirement(A.g2g.lr, 0, READ_ONLY, EXCLUSIVE, A.g2g.lr)
    );
    il.add_field(idx++, A.g2g.fid);
    // ... and go! Wait here for more accurate timings (at least we hope)... The
    // idea is that we want to separate initialization from the solve.
    lrt->execute_index_space(ctx, il).wait_all_results();
}

/**
 *
 */
inline void
setupHaloTask(const LegionRuntime::HighLevel::Task *task,
              const std::vector<LegionRuntime::HighLevel::PhysicalRegion> &rgns,
              LegionRuntime::HighLevel::Context ctx,
              LegionRuntime::HighLevel::HighLevelRuntime *lrt)
{
    using namespace LegionRuntime::HighLevel;
    using namespace LegionRuntime::Accessor;
    using LegionRuntime::Arrays::Rect;
    (void)ctx;
    static const uint8_t aMIdxsRID = 0;
    static const uint8_t g2gRID    = 1;
    // A (x2)
    assert(2 == rgns.size());
    // name the regions
    const PhysicalRegion &aipr = rgns[aMIdxsRID];
    const PhysicalRegion &ggpr = rgns[g2gRID];
    // convenience typedefs
    typedef RegionAccessor<AccessorType::Generic, int64_t>  GLRA;
    typedef RegionAccessor<AccessorType::Generic, I64Tuple> GTRA;
    // sparse matrix
    GLRA ai = aipr.get_field_accessor(0).typeify<int64_t>();
    const Domain aMIdxsDom = lrt->get_index_space_domain(
        ctx, task->regions[aMIdxsRID].region.get_index_space()
    );
    GTRA gg = ggpr.get_field_accessor(0).typeify<I64Tuple>();
    const Domain g2gDom = lrt->get_index_space_domain(
        ctx, task->regions[g2gRID].region.get_index_space()
    );
    // let the games begin...
    // Create a mapping between the real cell indicies in where they are
    // actually located in the global logical region. For example, a task may
    // need cell index 5, but 5 may actually be at cell 27. So, given an index
    // you need, I'll give you where it actually lives.
    Rect<1> mIdxRect = aMIdxsDom.get_rect<1>();
    Rect<1> g2gRect  = g2gDom.get_rect<1>();
    std::map<int64_t, int64_t> cellIDMap;
    for (GenericPointInRectIterator<1> itr(g2gRect); itr; itr++) {
        I64Tuple tmp = gg.read(DomainPoint::from_point<1>(itr.p));
        cellIDMap[tmp.b] = tmp.a;
    }
    for (GenericPointInRectIterator<1> itr(mIdxRect); itr; itr++) {
        // get old index
        int64_t index = ai.read(DomainPoint::from_point<1>(itr.p));
        // replace with real index
        ai.write(DomainPoint::from_point<1>(itr.p), cellIDMap[index]);
    }
}

} // end lgncg namespace

#endif
