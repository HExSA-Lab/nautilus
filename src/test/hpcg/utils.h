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

#ifndef LGNCG_UTILS_H_INCLUDED
#define LGNCG_UTILS_H_INCLUDED

#include "../../legion_runtime/legion.h"

namespace lgncg {

/**
 * convenience routine to get a task's ID
 */
static inline int
getTaskID(const LegionRuntime::HighLevel::Task *task)
{
    return task->index_point.point_data[0];
}

/**
 * courtesy of some other legion code.
 */
template <unsigned DIM, typename T>
static inline bool
offsetsAreDense(const Rect<DIM> &bounds,
                const LegionRuntime::Accessor::ByteOffset *offset)
{
#ifdef LGNCG_CHECK_OFF_R_DENSE
    off_t exp_offset = sizeof(T);
    for (unsigned i = 0; i < DIM; i++) {
        bool found = false;
        for (unsigned j = 0; j < DIM; j++)
            if (offset[j].offset == exp_offset) {
                found = true;
                exp_offset *= (bounds.hi[j] - bounds.lo[j] + 1);
                break;
            }
        if (!found) return false;
    }
    return true;
#else
    (void)bounds;
    (void)offset;
    return true;
#endif
}

/**
 * courtesy of some other legion code.
 */
static inline bool
offsetMismatch(int i,
               const LegionRuntime::Accessor::ByteOffset *off1,
               const LegionRuntime::Accessor::ByteOffset *off2)
{
    while (i-- > 0) {
        if ((off1++)->offset != (off2++)->offset) return true;
    }
    return false;
}

} // end lgncg namespace

#endif
