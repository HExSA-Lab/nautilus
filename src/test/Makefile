CXXFLAGS += -DSHARED_LOWLEVEL -DCOMPILE_TIME_MIN_LEVEL=LEVEL_DEBUG  -D__LEGION__
#-DDEBUG_LOWLEVEL -DDEBUG_HIGHLEVEL 

obj-y  = threads.o
obj-y += groups.o
obj-y += tasks.o
obj-y += futures.o
obj-y += bsp.o
obj-y += net_udp_echo.o
obj-y += test.o

obj-$(NAUT_CONFIG_PROVENANCE) += provenance.o

obj-$(NAUT_CONFIG_TEST_FIBERS) += fibers.o \
								   fibers_random.o

obj-$(NAUT_CONFIG_TEST_CACHEPART) += cachepart.o

obj-$(NAUT_CONFIG_OPENMP_RT_TESTS)  += openmp/
obj-$(NAUT_CONFIG_NDPC_RT_TESTS) += ndpc/
obj-$(NAUT_CONFIG_NESL_RT_TESTS) += nesl/

obj-$(NAUT_CONFIG_X86_64_HOST) += ipi.o
obj-$(NAUT_CONFIG_X86_64_HOST) += benchmark.o

obj-$(NAUT_CONFIG_GEM5) += ipi.o
obj-$(NAUT_CONFIG_GEM5) += benchmark.o

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
