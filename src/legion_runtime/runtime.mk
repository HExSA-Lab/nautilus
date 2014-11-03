# Copyright 2014 Stanford University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


ifndef LG_RT_DIR
$(error LG_RT_DIR variable is not defined, aborting build)
endif

CXX_FLAGS	+= -I$(LG_RT_DIR)

ifeq ($(strip $(DEBUG)),1)
CXX_FLAGS	+= -DDEBUG_LOW_LEVEL -DDEBUG_HIGH_LEVEL -ggdb #-ggdb -Wall
else
CXX_FLAGS	+= -O2 #-ggdb
endif


# Manage the output setting
CXX_FLAGS	+= -DCOMPILE_TIME_MIN_LEVEL=$(OUTPUT_LEVEL)


CXX_FLAGS	+= -DSHARED_LOWLEVEL
LOW_RUNTIME_SRC	+= $(LG_RT_DIR)/shared_lowlevel.cc 

# If you want to go back to using the shared mapper, comment out the next line
# and uncomment the one after that
MAPPER_SRC	+= $(LG_RT_DIR)/default_mapper.cc \
		   $(LG_RT_DIR)/shim_mapper.cc \
		   $(LG_RT_DIR)/mapping_utilities.cc
#MAPPER_SRC	+= $(LG_RT_DIR)/shared_mapper.cc
ifeq ($(strip $(ALT_MAPPERS)),1)
MAPPER_SRC	+= $(LG_RT_DIR)/alt_mappers.cc
endif

HIGH_RUNTIME_SRC += $(LG_RT_DIR)/legion.cc \
		    $(LG_RT_DIR)/legion_ops.cc \
		    $(LG_RT_DIR)/legion_tasks.cc \
		    $(LG_RT_DIR)/legion_trace.cc \
		    $(LG_RT_DIR)/legion_spy.cc \
		    $(LG_RT_DIR)/region_tree.cc \
		    $(LG_RT_DIR)/runtime.cc \
		    $(LG_RT_DIR)/garbage_collection.cc
