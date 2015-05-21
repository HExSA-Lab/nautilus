CXXFLAGS += -DSHARED_LOWLEVEL -DCOMPILE_TIME_MIN_LEVEL=LEVEL_DEBUG  -D__LEGION__
#-DDEBUG_LOWLEVEL -DDEBUG_HIGHLEVEL 
obj-$(NAUT_CONFIG_X86_64_HOST) += ipi.o
#obj-$(NAUT_CONFIG_X86_63_HOST) += benchmark.o
obj-$(NAUT_CONFIG_LEGION_RT) += test_legion.o \
								circuit.o \
								circuit_cpu.o \
								circuit_mapper.o 
								#saxpy_array.o 
								#delaunay.o 
								#hello_world.o \
								#partitioning.o \
								# custom_mapper.o \
								# multiple_partitions.o \
								# privileges.o \
								# physical_regions.o \
								# logical_regions.o \
								# global_vars.o \
								# index_tasks.o \
								# tasks_and_futures.o	\
