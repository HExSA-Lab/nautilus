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

#ifndef LGNCG_VECOR_H_INCLUDED
#define LGNCG_VECOR_H_INCLUDED

#include "../../legion_runtime/legion.h"

#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <inttypes.h>

namespace lgncg {

/**
 * partition vector item
 */
struct PVecItem {
    // a list of sub-grid bounds. provides a task ID to sub-grid bounds mapping
    std::vector< Rect<1> > subgridBnds;
    // launch domain
    LegionRuntime::HighLevel::Domain lDom;
    // logical partition
    LegionRuntime::HighLevel::LogicalPartition lPart;
    /**
     * constructor
     */
    PVecItem(const std::vector< Rect<1> > &sgb,
             const LegionRuntime::HighLevel::Domain &lDom,
             const LegionRuntime::HighLevel::LogicalPartition &lp)
        : subgridBnds(sgb), lDom(lDom), lPart(lp) { ; }
private:
    PVecItem(void);
};

struct Vector {
    // number of elements stored in the vector (the entire extent)
    int64_t len;
    // field ID
    LegionRuntime::HighLevel::FieldID fid;
    // the vector rect bounds
    LegionRuntime::Arrays::Rect<1> bounds;
    // logical region that holds vector values
    LegionRuntime::HighLevel::LogicalRegion lr;
private:
    // vec index space
    LegionRuntime::HighLevel::IndexSpace is;
    // vec field space
    LegionRuntime::HighLevel::FieldSpace fs;
    // vector of partition items
    std::vector<PVecItem> pvec;
    // the following are used for vector equality tests. that is, equality in
    // the "are these vectors the same from legion's perspective."
    id_t indexSpaceID;
    LegionRuntime::HighLevel::FieldSpaceID fieldSpaceID;
    LegionRuntime::HighLevel::RegionTreeID rTreeID;

public:
    template <typename T>
    void
    create(int64_t len,
           LegionRuntime::HighLevel::Context &ctx,
           LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        using namespace LegionRuntime::HighLevel;
        using LegionRuntime::Arrays::Rect;

        this->fid = 0;
        this->len = len;
        // create vals logical region
        // calculate the size of the lr vec (inclusive)
        int64_t n = len - 1;
        // vec rect
        this->bounds = Rect<1>(Point<1>::ZEROES(), Point<1>(n));
        // vector domain
        LegionRuntime::HighLevel::Domain dom(Domain::from_rect<1>(bounds));
        // vec index space
        this->is = lrt->create_index_space(ctx, dom);
        // vec field space
        this->fs = lrt->create_field_space(ctx);
        // vec field allocator
        FieldAllocator fa = lrt->create_field_allocator(ctx, fs);
        // all elements are going to be of size T
        fa.allocate_field(sizeof(T), fid);
        // now create the logical region
        this->lr = lrt->create_logical_region(ctx, is, fs);
        // at this point we don't have a logical partition
        // stash some info for equality checks
        indexSpaceID = this->lr.get_index_space().id;
        fieldSpaceID = this->lr.get_field_space().get_id();
        rTreeID      = this->lr.get_tree_id();
    }

    /**
     * cleans up and returns all allocated resources.
     */
    void
    free(LegionRuntime::HighLevel::Context &ctx,
         LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        typedef std::vector<PVecItem>::iterator VecIter;
        for (VecIter i = pvec.begin(); i != pvec.end(); i++) {
            lrt->destroy_logical_partition(ctx, i->lPart);
        }
        pvec.clear();
        lrt->destroy_logical_region(ctx, lr);
        lrt->destroy_field_space(ctx, fs);
        lrt->destroy_index_space(ctx, is);
    }

    /**
     *
     */
    static bool
    same(const Vector &a, const Vector &b) {
        return a.indexSpaceID == b.indexSpaceID &&
               a.fieldSpaceID == b.fieldSpaceID &&
               a.rTreeID      == b.rTreeID;
    }

    /**
     * returns current launch domain.
     */
    LegionRuntime::HighLevel::Domain
    lDom(size_t index = 0) const
    {
        const PVecItem &psi = pvec[index];
        return psi.lDom;
    }

    /**
     * returns current logical partition.
     */
    LegionRuntime::HighLevel::LogicalPartition
    lp(size_t index = 0) const
    {
        const PVecItem &psi = pvec[index];
        return psi.lPart;
    }

    /**
     * returns current sub-grid bounds.
     */
    std::vector< LegionRuntime::Arrays::Rect<1> >
    sgb(size_t index = 0) const
    {
        const PVecItem &psi = pvec[index];
        return psi.subgridBnds;
    }

    /**
     * FIXME desc pushes a partition scheme.
     */
    void
    partition(int64_t nParts,
              LegionRuntime::HighLevel::Context &ctx,
              LegionRuntime::HighLevel::HighLevelRuntime *lrt)
    {
        using namespace LegionRuntime::HighLevel;
        using LegionRuntime::Arrays::Rect;
        // FIXME for now, only allow even partitioning
        assert(0 == this->len % nParts);
        int64_t inc = this->len / nParts; // the increment
        Rect<1> colorBounds(Point<1>(0), Point<1>(nParts - 1));
        Domain colorDomain = Domain::from_rect<1>(colorBounds);
        //          +
        //          |
        //          |
        //     (x1)-+-+
        //          | |
        //          | m / nSubregions
        //     (x0) + |
        int64_t x0 = 0, x1 = inc - 1;
        DomainColoring disjointColoring;
        // a list of sub-grid bounds.
        // provides a task ID to sub-grid bounds mapping.
        std::vector< Rect<1> > subgridBnds;
        for (int64_t color = 0; color < nParts; ++color) {
            Rect<1> subRect((Point<1>(x0)), (Point<1>(x1)));
            // cache the subgrid bounds
            subgridBnds.push_back(subRect);
#if 0 // nice debug
            printf("vec disjoint partition: (%d) to (%d)\n",
                    subRect.lo.x[0], subRect.hi.x[0]);
#endif
            disjointColoring[color] = Domain::from_rect<1>(subRect);
            x0 += inc;
            x1 += inc;
        }
        IndexPartition iPart = lrt->create_index_partition(
                                   ctx, this->is,
                                   colorDomain, disjointColoring,
                                   true /* disjoint */
                               );
        // logical partitions
        using LegionRuntime::HighLevel::LogicalPartition;
        LogicalPartition lp = lrt->get_logical_partition(ctx, this->lr, iPart);
        // launch domain -- one task per color
        // launch domain
        LegionRuntime::HighLevel::Domain lDom = colorDomain;
        // add to the vector of partitions
        this->pvec.push_back(PVecItem(subgridBnds, lDom, lp));
    }

    /**
     * convenience routine that dumps the contents of this vector.
     */
    template<typename T>
    void
    dump(std::string prefix,
         int64_t nle,
         LegionRuntime::HighLevel::Context &ctx,
         LegionRuntime::HighLevel::HighLevelRuntime *lrt) const
    {
        using namespace LegionRuntime::HighLevel;
        using namespace LegionRuntime::Accessor;
        using LegionRuntime::Arrays::Rect;

        RegionRequirement req(lr, READ_ONLY, EXCLUSIVE, lr);
        req.add_field(fid);
        InlineLauncher dumpl(req);
        PhysicalRegion reg = lrt->map_region(ctx, dumpl);
        reg.wait_until_valid();
        typedef RegionAccessor<AccessorType::Generic, T> GTRA;
        GTRA acc = reg.get_field_accessor(fid).typeify<T>();
        typedef GenericPointInRectIterator<1> GPRI1D;
        typedef DomainPoint DomPt;
        std:: cout << "*** " << prefix << " ***" << std::endl;
        int i = 0;
        for (GPRI1D pi(bounds); pi; pi++, ++i) {
            T val = acc.read(DomPt::from_point<1>(pi.p));
            if (i % nle == 0) {
                std::cout << std::endl;
            }
            std::cout << std::setfill(' ') << std::setw(3) << val << " ";
        }
    }
};

} // end lgncg namespace

#endif
