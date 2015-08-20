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

#ifndef LGNCG_CG_MAPPER_H_INCLUDED
#define LGNCG_CG_MAPPER_H_INCLUDED

#include "../../legion_runtime/legion.h"
#include "../../legion_runtime/default_mapper.h"

#include <iostream>

enum {
    SUBREGION_TUNABLE
};

class CGMapper : public LegionRuntime::HighLevel::DefaultMapper {
    LegionRuntime::HighLevel::Memory localSysMem;
public:
    CGMapper(LegionRuntime::HighLevel::Machine *machine,
             LegionRuntime::HighLevel::HighLevelRuntime *rt,
             LegionRuntime::HighLevel::Processor p) :
        LegionRuntime::HighLevel::DefaultMapper(machine, rt, p) {
        using namespace LegionRuntime::HighLevel;
        using namespace LegionRuntime::Accessor;
        using namespace LegionRuntime::Arrays;
        const std::set<Processor> &allProcs = machine->get_all_processors();
        if ((*(allProcs.begin())) == local_proc) {
            printf("cgmapper: number of processors: %ld\n", allProcs.size());
            //
            const std::set<Memory> &allMems = machine->get_all_memories();
            printf("cgmapper: number of memories: %ld\n", allMems.size());
            //
            const std::set<Memory> visMems = machine->get_visible_memories(local_proc);
            printf("cgmapper: %ld memories visible from processor %x\n",
                    visMems.size(), local_proc.id);
        }
        localSysMem = machine_interface.find_memory_kind(
                           local_proc, Memory::SYSTEM_MEM
                      );
    }

#if 0
    virtual void
    select_task_options(Task *task);

    virtual void
    slice_domain(const Task *task, const Domain &domain,
                 std::vector<DomainSplit> &slices);

    virtual void
    notify_mapping_result(const Mappable *mappable);
#endif
    virtual bool
    map_task(LegionRuntime::HighLevel::Task *task);

    virtual bool
    map_copy(LegionRuntime::HighLevel::Copy *copy);

    virtual bool
    map_inline(LegionRuntime::HighLevel::Inline *inline_operation);

    virtual int
    get_tunable_value(const LegionRuntime::HighLevel::Task *task,
                      LegionRuntime::HighLevel::TunableID tid,
                      LegionRuntime::HighLevel::MappingTagID tag);
};

void
mapperRegistration(LegionRuntime::HighLevel::Machine *machine,
                    LegionRuntime::HighLevel::HighLevelRuntime *rt,
                    const std::set<LegionRuntime::HighLevel::Processor> &lProcs)
{
    using namespace LegionRuntime::HighLevel;
    using namespace std;
    for (set<Processor>::const_iterator it = lProcs.begin();
         it != lProcs.end(); it++) {
        rt->replace_default_mapper(new CGMapper(machine, rt, *it), *it);
    }
}

int
CGMapper::get_tunable_value(const LegionRuntime::HighLevel::Task *task,
                            LegionRuntime::HighLevel::TunableID tid,
                            LegionRuntime::HighLevel::MappingTagID tag)
{
    using namespace LegionRuntime::HighLevel;
    using std::set;

    if (SUBREGION_TUNABLE == tid) {
        const set<Processor> &cpuProcs =
            machine_interface.filter_processors(Processor::LOC_PROC);
        return cpuProcs.size();
    }
   // should never get here
   assert(false);
   return 0;
}

bool
CGMapper::map_task(LegionRuntime::HighLevel::Task *task)
{
    using namespace LegionRuntime::HighLevel;
    // put everything in the system memory
    assert(localSysMem.exists());
    for (unsigned idx = 0; idx < task->regions.size(); idx++) {
        task->regions[idx].target_ranking.push_back(localSysMem);
        task->regions[idx].virtual_map = false;
        task->regions[idx].enable_WAR_optimization = war_enabled;
        task->regions[idx].reduction_list = false;
        // make everything SOA
        task->regions[idx].blocking_factor =
            task->regions[idx].max_blocking_factor;
    }
    return true;
}

bool
CGMapper::map_copy(LegionRuntime::HighLevel::Copy *copy)
{
    using namespace LegionRuntime::HighLevel;
    using std::vector;

    vector<Memory> localStack;
    machine_interface.find_memory_stack(local_proc, localStack,
                                        (local_kind == Processor::LOC_PROC));
    assert(copy->src_requirements.size() == copy->dst_requirements.size());
    for (unsigned idx = 0; idx < copy->src_requirements.size(); idx++) {
        copy->src_requirements[idx].virtual_map = false;
        copy->src_requirements[idx].early_map = false;
        copy->src_requirements[idx].enable_WAR_optimization = war_enabled;
        copy->src_requirements[idx].reduction_list = false;
        copy->src_requirements[idx].make_persistent = false;
        if (!copy->src_requirements[idx].restricted) {
            copy->src_requirements[idx].target_ranking = localStack;
        }
        else {
            assert(copy->src_requirements[idx].current_instances.size() == 1);
            Memory target =
                copy->src_requirements[idx].current_instances.begin()->first;
            copy->src_requirements[idx].target_ranking.push_back(target);
        }
        copy->dst_requirements[idx].virtual_map = false;
        copy->dst_requirements[idx].early_map = false;
        copy->dst_requirements[idx].enable_WAR_optimization = war_enabled;
        copy->dst_requirements[idx].reduction_list = false;
        copy->dst_requirements[idx].make_persistent = false;

        if (!copy->dst_requirements[idx].restricted) {
            copy->dst_requirements[idx].target_ranking = localStack;
        }
        else {
            assert(copy->dst_requirements[idx].current_instances.size() == 1);
            Memory target =
                copy->dst_requirements[idx].current_instances.begin()->first;
            copy->dst_requirements[idx].target_ranking.push_back(target);
        }
        // make it SOA
        copy->src_requirements[idx].blocking_factor =
            copy->src_requirements[idx].max_blocking_factor;
        // make it SOA
        copy->dst_requirements[idx].blocking_factor =
            copy->dst_requirements[idx].max_blocking_factor;
    }
    // no profiling on copies yet
    return true;
}

bool
CGMapper::map_inline(LegionRuntime::HighLevel::Inline *inline_operation) {
    using namespace LegionRuntime::HighLevel;
    // let the default mapper do its thing,
    bool ret = DefaultMapper::map_inline(inline_operation);
    // then override the blocking factor to force SOA
    RegionRequirement& req = inline_operation->requirement;
    req.blocking_factor = req.max_blocking_factor;
    return ret;
}

#endif
