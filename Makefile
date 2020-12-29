VERSION = 0
PATCHLEVEL = 0
SUBLEVEL = 1
EXTRAVERSION = Nautilus
NAME=Nautilus


ISO_NAME:=nautilus.iso
BIN_NAME:=nautilus.bin
SYM_NAME:=nautilus.syms
SEC_NAME:=nautilus.secs
BC_NAME:=nautilus.bc
LL_NAME:=nautilus.ll



# *DOCUMENTATION*
# To see a list of typical targets execute "make help"
# More info can be located in ./README
# Comments in this file are targeted only to the developer, do not
# expect to learn how to build the kernel reading this file.

# Do not print "Entering directory ..."
MAKEFLAGS += --no-print-directory

# We are using a recursive build, so we need to do a little thinking
# to get the ordering right.
#
# Most importantly: sub-Makefiles should only ever modify files in
# their own directory. If in some directory we have a dependency on
# a file in another dir (which doesn't happen often, but it's often
# unavoidable when linking the built-in.o targets which finally
# turn into nautilus), we will call a sub make in that other dir, and
# after that we are sure that everything which is in that other dir
# is now up to date.
#
# The only cases where we need to modify files which have global
# effects are thus separated out and done before the recursive
# descending is started. They are now explicitly listed as the
# prepare rule.

# To put more focus on warnings, be less verbose as default
# Use 'make V=1' to see the full commands

ifdef V
  ifeq ("$(origin V)", "command line")
    KBUILD_VERBOSE = $(V)
  endif
endif
ifndef KBUILD_VERBOSE
  KBUILD_VERBOSE = 0
endif

# Call sparse as part of compilation of C files
# Use 'make C=1' to enable sparse checking

ifdef C
  ifeq ("$(origin C)", "command line")
    KBUILD_CHECKSRC = $(C)
  endif
endif
ifndef KBUILD_CHECKSRC
  KBUILD_CHECKSRC = 0
endif

# Use make M=dir to specify directory of external module to build
# Old syntax make ... SUBDIRS=$PWD is still supported
# Setting the environment variable KBUILD_EXTMOD take precedence
ifdef SUBDIRS
  KBUILD_EXTMOD ?= $(SUBDIRS)
endif
ifdef M
  ifeq ("$(origin M)", "command line")
    KBUILD_EXTMOD := $(M)
  endif
endif


# kbuild supports saving output files in a separate directory.
# To locate output files in a separate directory two syntaxes are supported.
# In both cases the working directory must be the root of the kernel src.
# 1) O=
# Use "make O=dir/to/store/output/files/"
# 
# 2) Set KBUILD_OUTPUT
# Set the environment variable KBUILD_OUTPUT to point to the directory
# where the output files shall be placed.
# export KBUILD_OUTPUT=dir/to/store/output/files/
# make
#
# The O= assignment takes precedence over the KBUILD_OUTPUT environment
# variable.


# KBUILD_SRC is set on invocation of make in OBJ directory
# KBUILD_SRC is not intended to be used by the regular user (for now)
ifeq ($(KBUILD_SRC),)

# OK, Make called in directory where kernel src resides
# Do we want to locate output files in a separate directory?
ifdef O
  ifeq ("$(origin O)", "command line")
    KBUILD_OUTPUT := $(O)
  endif
endif

# That's our default target when none is given on the command line
PHONY := _all
_all:

ifneq ($(KBUILD_OUTPUT),)
# Invoke a second make in the output directory, passing relevant variables
# check that the output directory actually exists
saved-output := $(KBUILD_OUTPUT)
KBUILD_OUTPUT := $(shell cd $(KBUILD_OUTPUT) && /bin/pwd)
$(if $(KBUILD_OUTPUT),, \
     $(error output directory "$(saved-output)" does not exist))

PHONY += $(MAKECMDGOALS)

$(filter-out _all,$(MAKECMDGOALS)) _all:
	$(if $(KBUILD_VERBOSE:1=),@)$(MAKE) -C $(KBUILD_OUTPUT) \
	KBUILD_SRC=$(CURDIR) \
	KBUILD_EXTMOD="$(KBUILD_EXTMOD)" -f $(CURDIR)/Makefile $@

# Leave processing to above invocation of make
skip-makefile := 1
endif # ifneq ($(KBUILD_OUTPUT),)
endif # ifeq ($(KBUILD_SRC),)

# We process the rest of the Makefile if this is the final invocation of make
ifeq ($(skip-makefile),)

# If building an external module we do not care about the all: rule
# but instead _all depend on modules
PHONY += all

_all: all


srctree		:= $(if $(KBUILD_SRC),$(KBUILD_SRC),$(CURDIR))
TOPDIR		:= $(srctree)
# FIXME - TOPDIR is obsolete, use srctree/objtree
objtree		:= $(CURDIR)
src		:= $(srctree)
obj		:= $(objtree)

VPATH		:= $(srctree)$(if $(KBUILD_EXTMOD),:$(KBUILD_EXTMOD))

export srctree objtree VPATH TOPDIR


# SUBARCH tells the usermode build what the underlying arch is.  That is set
# first, and if a usermode build is happening, the "ARCH=um" on the command
# line overrides the setting of ARCH below.  If a native build is happening,
# then ARCH is assigned, getting whatever value it gets normally, and 
# SUBARCH is subsequently ignored.

SUBARCH := $(shell uname -m | sed -e s/i.86/i386/  )

# Cross compiling and selecting different set of gcc/bin-utils
# ---------------------------------------------------------------------------
#
# When performing cross compilation for other architectures ARCH shall be set
# to the target architecture. (See arch/* for the possibilities).
# ARCH can be set during invocation of make:
# make ARCH=ia64
# Another way is to have ARCH set in the environment.
# The default ARCH is the host where make is executed.

# CROSS_COMPILE specify the prefix used for all executables used
# during compilation. Only gcc and related bin-utils executables
# are prefixed with $(CROSS_COMPILE).
# CROSS_COMPILE can be set on the command line
# make CROSS_COMPILE=ia64-linux-
# Alternatively CROSS_COMPILE can be set in the environment.
# Default value for CROSS_COMPILE is not to prefix executables
# Note: Some architectures assign CROSS_COMPILE in their arch/*/Makefile

ARCH		?= $(SUBARCH)
CROSS_COMPILE	?= 
#CROSS_COMPILE	?= /home/kyle/opt/cross/bin/x86_64-elf-

# Architecture as present in compile.h
UTS_MACHINE := $(ARCH)

# SHELL used by kbuild
CONFIG_SHELL := $(shell if [ -x "$$BASH" ]; then echo $$BASH; \
	  else if [ -x /bin/bash ]; then echo /bin/bash; \
	  else echo sh; fi ; fi)

HOSTCC      := gcc
HOSTCXX     := g++

HOSTCFLAGS	= -Wall -Wstrict-prototypes  -fomit-frame-pointer \
			-Wno-unused -Wno-format-security -U_FORTIFY_SOURCE
HOSTCXXFLAGS	= -O

# 	Decide whether to build built-in, modular, or both.
#	Normally, just do built-in.

KBUILD_MODULES :=
KBUILD_BUILTIN := 1

#	If we have only "make modules", don't compile built-in objects.
#	When we're building modules with modversions, we need to consider
#	the built-in objects during the descend as well, in order to
#	make sure the checksums are uptodate before we record them.

ifeq ($(MAKECMDGOALS),modules)
  KBUILD_BUILTIN := $(if $(CONFIG_MODVERSIONS),1)
endif

#	If we have "make <whatever> modules", compile modules
#	in addition to whatever we do anyway.
#	Just "make" or "make all" shall build modules as well

ifneq ($(filter all _all modules,$(MAKECMDGOALS)),)
  KBUILD_MODULES := 1
endif

ifeq ($(MAKECMDGOALS),)
  KBUILD_MODULES := 1
endif

export KBUILD_MODULES KBUILD_BUILTIN
export KBUILD_CHECKSRC KBUILD_SRC KBUILD_EXTMOD

# Beautify output
# ---------------------------------------------------------------------------
#
# Normally, we echo the whole command before executing it. By making
# that echo $($(quiet)$(cmd)), we now have the possibility to set
# $(quiet) to choose other forms of output instead, e.g.
#
#         quiet_cmd_cc_o_c = Compiling $(RELDIR)/$@
#         cmd_cc_o_c       = $(CC) $(c_flags) -c -o $@ $<
#
# If $(quiet) is empty, the whole command will be printed.
# If it is set to "quiet_", only the short version will be printed. 
# If it is set to "silent_", nothing wil be printed at all, since
# the variable $(silent_cmd_cc_o_c) doesn't exist.
#
# A simple variant is to prefix commands with $(Q) - that's useful
# for commands that shall be hidden in non-verbose mode.
#
#	$(Q)ln $@ :<
#
# If KBUILD_VERBOSE equals 0 then the above command will be hidden.
# If KBUILD_VERBOSE equals 1 then the above command is displayed.

ifeq ($(KBUILD_VERBOSE),1)
  quiet =
  Q =
else
  quiet=quiet_
  Q = @
endif

# If the user is running make -s (silent mode), suppress echoing of
# commands

ifneq ($(findstring s,$(MAKEFLAGS)),)
  quiet=silent_
endif

export quiet Q KBUILD_VERBOSE


# Look for make include files relative to root of kernel src
MAKEFLAGS += --include-dir=$(srctree)

# We need some generic definitions
include  $(srctree)/scripts/Kbuild.include

# For maximum performance (+ possibly random breakage, uncomment
# the following)

#MAKEFLAGS += -rR

# Make variables (CC, etc...)


AR		= $(CROSS_COMPILE)ar
NM		= $(CROSS_COMPILE)nm
STRIP		= $(CROSS_COMPILE)strip
OBJCOPY		= $(CROSS_COMPILE)objcopy
OBJDUMP		= $(CROSS_COMPILE)objdump

CPP		= $(CC) -E
GRUBMKRESCUE    = $(CROSS_COMPILE)grub-mkrescue
AWK		= awk
GENKSYMS	= scripts/genksyms/genksyms
DEPMOD		= /sbin/depmod
KALLSYMS	= scripts/kallsyms
PERL		= perl
CHECK		= sparse


CFLAGS_KERNEL	=
AFLAGS_KERNEL	=


# Use NAUT_INCLUDE when you must reference the include/ directory.
# Needed to be compatible with the O= option
NAUT_INCLUDE      := -D__NAUTILUS__ -Iinclude \
                   $(if $(KBUILD_SRC),-I$(srctree)/include) \
		   -include include/autoconf.h

CPPFLAGS        := $(NAUT_INCLUDE) -D__NAUTILUS__


#
# Include .config early so that we have access to the toolchain-related options
#
ifeq (,$(wildcard .config))
   NAUT_CONFIG_USE_GCC=1
   NAUT_CONFIG_USE_CLANG=0
   NAUT_CONFIG_USE_WLLVM=0
   NAUT_CONFIG_COMPILER_PREFIX=
   NAUT_CONFIG_COMPILER_SUFFIX=
else
   include .config
endif

COMPILER_PREFIX := $(patsubst "%",%,$(NAUT_CONFIG_COMPILER_PREFIX))
COMPILER_SUFFIX := $(patsubst "%",%,$(NAUT_CONFIG_COMPILER_SUFFIX))


#
# Note we use the system linker in all cases
#
ifdef NAUT_CONFIG_USE_CLANG
  AS		= $(CROSS_COMPILE)$(COMPILER_PREFIX)llvm-as$(COMPILER_SUFFIX)
  LD		= $(CROSS_COMPILE)ld
  CC		= $(CROSS_COMPILE)$(COMPILER_PREFIX)clang$(COMPILER_SUFFIX)
  CXX           = $(CROSS_COMPILE)$(COMPILER_PREFIX)clang++$(COMPILER_SUFFIX)
endif

ifdef NAUT_CONFIG_USE_WLLVM
  AS		= $(CROSS_COMPILE)$(COMPILER_PREFIX)llvm-as$(COMPILER_SUFFIX)
  LD		= $(CROSS_COMPILE)ld
  CC		= $(CROSS_COMPILE)$(COMPILER_PREFIX)wllvm$(COMPILER_SUFFIX)
  CXX           = $(CROSS_COMPILE)$(COMPILER_PREFIX)wllvm++$(COMPILER_SUFFIX)
endif

ifdef NAUT_CONFIG_USE_GCC
  AS		= $(CROSS_COMPILE)$(COMPILER_PREFIX)as$(COMPILER_SUFFIX)
  LD		= $(CROSS_COMPILE)ld
  CC		= $(CROSS_COMPILE)$(COMPILER_PREFIX)gcc$(COMPILER_SUFFIX)
  CXX           = $(CROSS_COMPILE)$(COMPILER_PREFIX)g++$(COMPILER_SUFFIX)
endif



COMMON_FLAGS :=-fno-omit-frame-pointer \
			   -ffreestanding \
			   -fno-stack-protector \
			   -fno-strict-aliasing \
                           -fno-strict-overflow \
			   -mno-red-zone \
			   -mcmodel=large



ifdef NAUT_CONFIG_USE_GCC
  COMMON_FLAGS += -O2  -fno-delete-null-pointer-checks
  GCCVERSIONGTE6 := $(shell expr `$(CC) -dumpversion | cut -f1 -d.` \>= 6)
  ifeq "$(GCCVERSIONGTE6)" "1"
    COMMON_FLAGS += -no-pie -fno-pic -fno-PIC -fno-PIE
  endif

# recent versions of gcc (6+) have position independence
# as a default, and this breaks us
endif

ifdef NAUT_CONFIG_USE_CLANG
  COMMON_FLAGS += -O2  # -fno-delete-null-pointer-checks
   # -O3 will also work - PAD
endif

ifdef NAUT_CONFIG_USE_WLLVM
  COMMON_FLAGS += -O2  # -fno-delete-null-pointer-checks
   # -O3 will also work - PAD
endif

#
# Add these for more recent compilers to avoid having
# the compiler insert surprise ud2 instructions for you should
# you derefence address zero
#
#                           -fno-isolate-erroneous-paths-attribute \
#                           -fno-isolate-erroneous-paths-dereference \
#			  
#
# 
# For testing, optionally add -fsanitize=undefined
#
#
#

CXXFLAGS := $(COMMON_FLAGS) \
			-fno-exceptions \
			-fno-rtti 

CFLAGS:=   $(COMMON_FLAGS) \
		   -Wall \
		   -Wno-unused-function \
		   -Wno-unused-variable \
		   -fno-common \
		   -Wstrict-overflow=5 

#                   -Wextra \
#                   -Wpedantic \
#

# if we're using Clang, we can't use these
ifdef NAUT_CONFIG_USE_GCC
CFLAGS += -std=gnu99 \
		  -Wno-frame-address \
		  $(call cc-option, -Wno-unused-but-set-variable,) 
endif

ifeq ($(call cc-option-yn, -fgnu89-inline),y)
CFLAGS		+= -fgnu89-inline
endif



# NOTE: We MUST have max-page-size set to this here. Otherwise things
# go off the rails for the Grub multiboot setup because the linker
# does strange things...
LDFLAGS         := -z max-page-size=0x1000
AFLAGS		:= 

# Read KERNELRELEASE from .kernelrelease (if it exists)
#KERNELRELEASE = $(shell cat .kernelrelease 2> /dev/null)
#KERNELVERSION = $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)

export	VERSION PATCHLEVEL SUBLEVEL KERNELRELEASE KERNELVERSION \
	ARCH CONFIG_SHELL HOSTCC HOSTCFLAGS CROSS_COMPILE AS LD CC CXX\
	CPP AR NM STRIP OBJCOPY OBJDUMP MAKE AWK GENKSYMS PERL UTS_MACHINE \
	HOSTCXX HOSTCXXFLAGS CHECK \
export CPPFLAGS NOSTDINC_FLAGS NAUT_INCLUDE OBJCOPYFLAGS LDFLAGS \
export CFLAGS CXXFLAGS CFLAGS_KERNEL \
export AFLAGS AFLAGS_KERNEL \

# When compiling out-of-tree modules, put MODVERDIR in the module
# tree rather than in the kernel tree. The kernel tree might
# even be read-only.
export MODVERDIR := $(if $(KBUILD_EXTMOD),$(firstword $(KBUILD_EXTMOD))/).tmp_versions

# Files to ignore in find ... statements

RCS_FIND_IGNORE := \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS -o -name .pc -o -name .hg -o -name .git \) -prune -o
export RCS_TAR_IGNORE := --exclude SCCS --exclude BitKeeper --exclude .svn --exclude CVS --exclude .pc --exclude .hg --exclude .git

# ===========================================================================
# Rules shared between *config targets and build targets

# Basic helpers built in scripts/
PHONY += scripts_basic
scripts_basic:
	$(Q)$(MAKE) $(build)=scripts/basic

# To avoid any implicit rule to kick in, define an empty command.
scripts/basic/%: scripts_basic ;

PHONY += outputmakefile
# outputmakefile generates a Makefile in the output directory, if using a
# separate output directory. This allows convenient use of make in the
# output directory.
outputmakefile:
ifneq ($(KBUILD_SRC),)
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkmakefile \
	    $(srctree) $(objtree) $(VERSION) $(PATCHLEVEL)
endif

# To make sure we do not include .config for any of the *config targets
# catch them early, and hand them over to scripts/kconfig/Makefile
# It is allowed to specify more targets when calling make, including
# mixing *config targets and build targets.
# For example 'make oldconfig all'. 
# Detect when mixed targets is specified, and make a second invocation
# of make so .config is not included in this case either (for *config).

no-dot-config-targets := clean mrproper distclean \
			 cscope TAGS tags help %docs check%

config-targets := 0
mixed-targets  := 0
dot-config     := 1

ifneq ($(filter $(no-dot-config-targets), $(MAKECMDGOALS)),)
	ifeq ($(filter-out $(no-dot-config-targets), $(MAKECMDGOALS)),)
		dot-config := 0
	endif
endif

ifeq ($(KBUILD_EXTMOD),)
        ifneq ($(filter %config,$(MAKECMDGOALS)),)
                config-targets := 1
                ifneq ($(filter-out %config,$(MAKECMDGOALS)),)
                        mixed-targets := 1
                endif
        endif
endif

ifeq ($(mixed-targets),1)
# ===========================================================================
# We're called with mixed targets (*config and build targets).
# Handle them one by one.

%:: FORCE
	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= $@

else
ifeq ($(config-targets),1)
# ===========================================================================
# *config targets only - make sure prerequisites are updated, and descend
# in scripts/kconfig to make the *config target

# Read arch specific Makefile to set KBUILD_DEFCONFIG as needed.
# KBUILD_DEFCONFIG may point out an alternative default configuration
# used for 'make defconfig'
include $(srctree)/Makefile.$(ARCH)
export KBUILD_DEFCONFIG

%config: scripts_basic outputmakefile FORCE
	$(Q)mkdir -p include/config
	$(Q)$(MAKE) $(build)=scripts/kconfig $@
#	$(Q)$(MAKE) -C $(srctree) KBUILD_SRC= .kernelrelease

else
# ===========================================================================
# Build targets only - this includes Nautilus arch specific targets, clean
# targets and others. In general all targets except *config targets.


# Additional helpers built in scripts/
# Carefully list dependencies so we do not try to build scripts twice
# in parrallel
PHONY += scripts
scripts: scripts_basic include/config/MARKER
	$(Q)$(MAKE) $(build)=$(@)

scripts_basic: include/autoconf.h

# Objects we will link into Nautilus / subdirs we need to visit
core-y          := src/
libs-y		    := lib/ 


ifeq ($(dot-config),1)
# In this section, we need .config

# Read in dependencies to all Kconfig* files, make sure to run
# oldconfig if changes are detected.
-include .kconfig.d

include .config

# If .config needs to be updated, it will be done via the dependency
# that autoconf has on .config.
# To avoid any implicit rule to kick in, define an empty command
.config .kconfig.d: ;

# If .config is newer than include/autoconf.h, someone tinkered
# with it and forgot to run make oldconfig.
# If kconfig.d is missing then we are probably in a cleaned tree so
# we execute the config step to be sure to catch updated Kconfig files
include/autoconf.h: .kconfig.d .config
	$(Q)mkdir -p include/config
	$(Q)$(MAKE) -f $(srctree)/Makefile silentoldconfig
else
# Dummy target needed, because used as prerequisite
include/autoconf.h: ;
endif



ifdef NAUT_CONFIG_XEON_PHI
AS		= $(CROSS_COMPILE)k1om-mpss-linux-as
LD		= $(CROSS_COMPILE)k1om-mpss-linux-ld
CC		= $(CROSS_COMPILE)k1om-mpss-linux-gcc
CXX             = $(CROSS_COMPILE)k1om-mpss-linux-g++
AR		= $(CROSS_COMPILE)k1om-mpss-linux-ar
NM		= $(CROSS_COMPILE)k1om-mpss-linux-nm
STRIP		= $(CROSS_COMPILE)k1om-mpss-linux-strip
OBJCOPY		= $(CROSS_COMPILE)k1om-mpss-linux-objcopy
OBJDUMP		= $(CROSS_COMPILE)k1om-mpss-linux-objdump
CFLAGS += -fno-optimize-sibling-calls
endif

#
# Update libs, etc based on NAUT_CONFIG_TOOLCHAIN_ROOT 
#
#
ifneq ($(NAUT_CONFIG_CXX_SUPPORT)a,a)
ifeq ($(NAUT_CONFIG_TOOLCHAIN_ROOT)a,""a)
ifeq ($(CROSS_COMPILE)a, a)
# guess where the std libs are 
  libs-y += `locate libstdc++.a | head -1`
else
  libs-y += $(CROSS_COMPILE)/../lib64/libstdc++.a
endif
else
  libs-y += $(NAUT_CONFIG_TOOLCHAIN_ROOT)/lib64/libstdc++.a
endif
endif # NAUT_CONFIG_CXX_SUPPORT

                           #$(NAUT_CONF_TOOLCHAIN_ROOT)/lib64/libstdc++.a
			   #-lstdc++ 
                           #/usr/lib64/libstdc++.a \
			   #/home/kyle/opt/cross/lib/gcc/x86_64-elf/4.8.0/libgcc.a \
			   #/usr/lib64/libsupc++.a \

			   #/usr/lib64/libc.a \

ifdef NAUT_CONFIG_OPENMP_RT_OMP
   libs-y += $(NAUT_CONFIG_OPENMP_RT_INSTALL_DIR)/lib/libomp.a
endif

ifdef NAUT_CONFIG_PALACIOS
  PALACIOS_DIR=$(subst "",,$(NAUT_CONFIG_PALACIOS_DIR))
  CFLAGS += -I$(PALACIOS_DIR)/nautilus -I$(PALACIOS_DIR)/palacios/include
  libs-y += $(PALACIOS_DIR)/libv3vee.a $(PALACIOS_DIR)/libnautilus.a
  LDFLAGS         +=  --whole-archive -dp
# image attachement here somewhere for testing, probably via a linker script
endif


# The all: target is the default when no target is given on the
# command line.
# This allow a user to issue only 'make' to build a kernel including modules
# Defaults nautilus but it is usually overriden in the arch makefile
all: nautilus $(DEFAULT_EXTRA_TARGETS)

ifdef NAUT_CONFIG_DEBUG_INFO
CFLAGS		+= -g
endif

include $(srctree)/Makefile.$(ARCH)

# arch Makefile may override CC so keep this after arch Makefile is included
NOSTDINC_FLAGS +=


# disable pointer signedness warnings in gcc 4.0
CFLAGS += $(call cc-option,-Wno-pointer-sign,)

# Default kernel image to build when no specific target is given.
# KBUILD_IMAGE may be overruled on the commandline or
# set in the environment
# Also any assignments in /Makefile.$(ARCH) take precedence over
# this default value
export KBUILD_IMAGE ?= nautilus

#
# INSTALL_PATH specifies where to place the updated kernel and system map
# images. Default is /boot, but you can set it to other values
export	INSTALL_PATH ?= /build


nautilus-dirs	:= $(patsubst %/,%,$(filter %/,  \
		     $(core-y) $(libs-y)))

nautilus-cleandirs := $(sort $(nautilus-dirs) $(patsubst %/,%,$(filter %/, \
		     	$(core-n) $(core-) $(libs-n) $(libs-))))

core-y		:= $(patsubst %/, %/built-in.o, $(core-y))
libs-y		:= $(patsubst %/, %/built-in.o, $(libs-y))


# Build Nautilus
# ---------------------------------------------------------------------------
# Nautilus is build from the objects selected by $(nautilus-init) and
# $(nautilus). Most are built-in.o files from top-level directories
# in the kernel tree, others are specified in Makefile.
# Ordering when linking is important, and $(nautilus-init) must be first.
#
# Nautilus 
#   ^
#   |
#   +--< $(nautilus)
#   |    +--< src/built-in.o  + more
#


nautilus := $(core-y) $(libs-y) 

ifdef NAUT_CONFIG_XEON_PHI
LD_SCRIPT:=link/nautilus.ld.xeon_phi
else 
ifdef NAUT_CONFIG_HVM_HRT
LD_SCRIPT:=link/nautilus.ld.hrt
else
ifdef NAUT_CONFIG_PALACIOS
LD_SCRIPT:=link/nautilus.ld.palacios
else
ifdef NAUT_CONFIG_GEM5
LD_SCRIPT:=link/nautilus.ld.gem5
else
LD_SCRIPT:=link/nautilus.ld
endif
endif
endif
endif

quiet_cmd_transform_linkscript__ ?= CC      $@
	  cmd_transform_linkscript__ ?= $(CC) -E -P \
	  -DHIHALF_OFFSET=$(NAUT_CONFIG_HRT_HIHALF_OFFSET) \
	  -x c-header -o link/nautilus.ld.hrt link/hrt.lds

# Rule to link nautilus - also used during CONFIG_CONFIGKALLSYMS
# May be overridden by /Makefile.$(ARCH)
quiet_cmd_nautilus__ ?= LD      $@
      cmd_nautilus__ ?= $(LD) $(LDFLAGS) $(LDFLAGS_vmlinux) -o $@ \
      -T $(LD_SCRIPT) $(core-y)  \
      --start-group $(libs-y) --end-group

# Generate new nautilus version
quiet_cmd_nautilus_version = GEN     .version
      cmd_nautilus_version = set -e;                       \
	if [ ! -r .version ]; then			\
	  rm -f .version;				\
	  echo 1 >.version;				\
        else						\
	  mv .version .old_version;			\
	  expr 0$$(cat .old_version) + 1 >.version;	\
	fi;					


# Link of nautilus 
# If CONFIG_KALLSYMS is set .version is already updated
# Generate System.map and verify that the content is consistent
# Use + in front of the nautilus rule to silent warning with make -j2
# First command is ':' to allow us to use + in front of the rule
ifdef NAUT_CONFIG_HVM_HRT
define rule_nautilus__
	:
	$(call cmd,transform_linkscript__)
	$(call cmd,nautilus__)
	$(Q)echo 'cmd_$@ := $(cmd_nautilus__)' > $(@D)/.$(@F).cmd
endef
else
define rule_nautilus__
	:
	$(call cmd,nautilus__)
	$(Q)echo 'cmd_$@ := $(cmd_nautilus__)' > $(@D)/.$(@F).cmd
endef
endif


# nautilus image - including updated kernel symbols
$(BIN_NAME): $(nautilus)
	$(call if_changed_rule,nautilus__)

$(SYM_NAME): $(BIN_NAME)
	@scripts/gen_sym_file.sh $(BIN_NAME) tmp.sym

$(SEC_NAME): $(BIN_NAME)
	@scripts/gen_sec_file.sh $(BIN_NAME) tmp.sec

nautilus: $(BIN_NAME) $(SYM_NAME) $(SEC_NAME)

# New function to run a Python script which generates Lua test code,
# addition of a separate flag (LUA_BUILD_FLAG) which is set to indicate
# that the debug symbols we need for wrapping Nautilus functions will
# be available during the second build (which we need to generate appropriate 
# function wrappers)
define lua__
	@python scripts/parse_gdb.py

	LUA_BUILD_FLAG=1 make
endef

# if NAUT_CONFIG_LUA_TEST is defined then call the function lua__ to
# generate additional wrapper code
#

quiet_cmd_isoimage = MKISO   $(ISO_NAME)
define cmd_isoimage 
	mkdir -p .iso/boot/grub && \
	cp configs/grub.cfg .iso/boot/grub && \
	cp $(BIN_NAME) $(SYM_NAME) $(SEC_NAME) .iso/boot && \
	$(GRUBMKRESCUE) -o $(ISO_NAME) .iso >/dev/null 2>&1 && \
	rm -rf .iso
endef


isoimage: nautilus
ifdef NAUT_CONFIG_LUA_TEST
	$(call lua__)
endif
	$(call cmd,isoimage)

nautilus.asm: $(BIN_NAME)
	$(OBJDUMP) --disassemble $< > $@

ifdef NAUT_CONFIG_USE_WLLVM
bitcode: $(BIN_NAME)
	extract-bc $(BIN_NAME) -o $(BC_NAME)
	llvm-dis $(BC_NAME) -o $(LL_NAME)

ifdef NAUT_CONFIG_USE_WLLVM_WHOLE_OPT
whole_opt: $(BIN_NAME)  
	extract-bc $(BIN_NAME) -o $(BC_NAME)
	opt -strip-debug $(BC_NAME)
	clang $(CFLAGS) -c $(BC_NAME) -o .nautilus.o
	$(LD) $(LDFLAGS) $(LDFLAGS_vmlinux) -o $(BIN_NAME) -T $(LD_SCRIPT) .nautilus.o `scripts/findasm.pl`
	rm .nautilus.o
endif

endif
# The actual objects are generated when descending, 
# make sure no implicit rule kicks in
$(sort  $(nautilus)) : $(nautilus-dirs) ;

# Handle descending into subdirectories listed in $(nautilus-dirs)
# Preset locale variables to speed up the build process. Limit locale
# tweaks to this spot to avoid wrong language settings when running
# make menuconfig etc.
# Error messages still appears in the original language

PHONY += $(nautilus-dirs)
$(nautilus-dirs): prepare scripts
	$(Q)$(MAKE) $(build)=$@


# Things we need to do before we recursively start building the kernel
# or the modules are listed in "prepare".
# A multi level approach is used. prepareN is processed before prepareN-1.
# archprepare is used in arch Makefiles and when processed arch symlink,
# version.h and scripts_basic is processed / created.

# Listed in dependency order
PHONY += prepare archprepare prepare0 prepare1 prepare2 prepare3

# prepare-all is deprecated, use prepare as valid replacement
PHONY += prepare-all

# prepare3 is used to check if we are building in a separate output directory,
# and if so do:
# 1) Check that make has not been executed in the kernel src $(srctree)
prepare3: 
ifneq ($(KBUILD_SRC),)
	@echo '  Using $(srctree) as source for kernel'
	$(Q)if [ -f $(srctree)/.config ]; then \
		echo "  $(srctree) is not clean, please run 'make mrproper'";\
		echo "  in the '$(srctree)' directory.";\
		/bin/false; \
	fi;
endif

# prepare2 creates a makefile if using a separate output directory
prepare2: prepare3 outputmakefile

prepare1: prepare2 include/config/MARKER

archprepare: prepare1 scripts_basic

prepare0: archprepare FORCE
	$(Q)$(MAKE) $(build)=.

# All the preparing..
prepare prepare-all: prepare0



include/config/MARKER: scripts/basic/split-include include/autoconf.h
	@echo '  SPLIT   include/autoconf.h -> include/config/*'
	@scripts/basic/split-include include/autoconf.h include/config
	@touch $@



# ---------------------------------------------------------------------------

PHONY += depend dep
depend dep:
	@echo '*** Warning: make $@ is unnecessary now.'



###
# Cleaning is done on three levels.
# make clean     Delete most generated files
#                Leave enough to build external modules
# make mrproper  Delete the current configuration, and all generated files
# make distclean Remove editor backup files, patch leftover files and the like

# Directories & files removed with 'make clean'
CLEAN_DIRS  += $(MODVERDIR)
CLEAN_FILES +=	 nautilus.asm $(SYM_NAME) $(SEC_NAME) $(ISO_NAME) $(BIN_NAME) \
                 .tmp_version .tmp_nautilus*

# Directories & files removed with 'make mrproper'
MRPROPER_DIRS  += include/config 
MRPROPER_FILES += .config .config.old  .version .old_version \
                  include/autoconf.h  \
		  tags TAGS cscope*


#	 	\( -name '*.[oas]' -o -name '*.ko' -o -name '.*.cmd' \


# clean - Delete most, but leave enough to build external modules
#
clean: rm-dirs  := $(CLEAN_DIRS)
clean: rm-files := $(CLEAN_FILES)
clean-dirs      := $(addprefix _clean_,$(srctree) $(nautilus-cleandirs))

PHONY += $(clean-dirs) clean archclean
$(clean-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _clean_%,%,$@)


clean: archclean $(clean-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)
	@find . $(RCS_FIND_IGNORE) \
		\( -name 'lib' \) -prune -o \
	 	\( -name '*.[oas]' -o -name '.*.cmd' \
		-o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' -o -name '*.ko' \) \
		-type f -print | xargs rm -f

# mrproper - Delete all generated files, including .config
#
mrproper: rm-dirs  := $(wildcard $(MRPROPER_DIRS))
mrproper: rm-files := $(wildcard $(MRPROPER_FILES))
#mrproper-dirs      := $(addprefix _mrproper_,Documentation/DocBook scripts)
mrproper-dirs      := $(addprefix _mrproper_, scripts)

PHONY += $(mrproper-dirs) mrproper archmrproper
$(mrproper-dirs):
	$(Q)$(MAKE) $(clean)=$(patsubst _mrproper_%,%,$@)

mrproper: clean archmrproper $(mrproper-dirs)
	$(call cmd,rmdirs)
	$(call cmd,rmfiles)

# distclean
#
PHONY += distclean

distclean: mrproper
	@find $(srctree) $(RCS_FIND_IGNORE) \
	 	\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
	 	-o -name '.*.rej' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \) \
		-type f -print | xargs rm -f


# Packaging of the kernel to various formats
# ---------------------------------------------------------------------------
# rpm target kept for backward compatibility
package-dir	:= $(srctree)/scripts/package

%pkg: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@
rpm: FORCE
	$(Q)$(MAKE) $(build)=$(package-dir) $@


# Brief documentation of the typical targets used
# ---------------------------------------------------------------------------


help:
	@echo  'Cleaning targets:'
	@echo  '  clean		  - remove most generated files but keep the config'
	@echo  '  mrproper	  - remove all generated files + config + various backup files'
	@echo  ''
	@echo  'Configuration targets:'
	@$(MAKE) -f $(srctree)/scripts/kconfig/Makefile help
	@echo  ''
	@echo  'Other generic targets:'
	@echo  '  all		  - Build all targets marked with [*]'
	@echo  '* nautilus - Build the Nautilus Kernel'
	@echo  '  dir/            - Build all files in dir and below'
	@echo  '  dir/file.[ois]  - Build specified target only'
	@echo  '  dir/file.ko     - Build module including final link'
	@echo  '  rpm		  - Build a kernel as an RPM package'
	@echo  '  tags/TAGS	  - Generate tags file for editors'
	@echo  '  cscope	  - Generate cscope index'
	@echo  ''
	@echo  '  make V=0|1 [targets] 0 => quiet build (default), 1 => verbose build'
	@echo  '  make O=dir [targets] Locate all output files in "dir", including .config'
	@echo  '  make C=1   [targets] Check all c source with $$CHECK (sparse)'
	@echo  '  make C=2   [targets] Force check of all c source with $$CHECK (sparse)'
	@echo  ''
	@echo  'Execute "make" or "make all" to build all targets marked with [*] '
	@echo  'For further info see the ./README file'


# Documentation targets
# ---------------------------------------------------------------------------
%docs: scripts_basic FORCE
	$(Q)$(MAKE) $(build)=Documentation/DocBook $@


# Generate tags for editors
# ---------------------------------------------------------------------------

#We want __srctree to totally vanish out when KBUILD_OUTPUT is not set
#(which is the most common case IMHO) to avoid unneeded clutter in the big tags file.
#Adding $(srctree) adds about 20M on i386 to the size of the output file!

ifeq ($(src),$(obj))
__srctree =
else
__srctree = $(srctree)/
endif

ifeq ($(ALLSOURCE_ARCHS),)
ifeq ($(ARCH),um)
ALLINCLUDE_ARCHS := $(ARCH) $(SUBARCH)
else
ALLINCLUDE_ARCHS := $(ARCH)
endif
else
#Allow user to specify only ALLSOURCE_PATHS on the command line, keeping existing behaviour.
ALLINCLUDE_ARCHS := $(ALLSOURCE_ARCHS)
endif

ALLSOURCE_ARCHS := $(ARCH)

define all-sources
	( find $(__srctree)nautilus $(RCS_FIND_IGNORE) \
	       \( -name lib \) -prune -o \
	       -name '*.[chS]' -print; )
endef

quiet_cmd_cscope-file = FILELST cscope.files
      cmd_cscope-file = (echo \-k; echo \-q; $(all-sources)) > cscope.files

quiet_cmd_cscope = MAKE    cscope.out
      cmd_cscope = cscope -b

cscope: FORCE
	$(call cmd,cscope-file)
	$(call cmd,cscope)

quiet_cmd_TAGS = MAKE   $@
define cmd_TAGS
	rm -f $@; \
	ETAGSF=`etags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs etags $$ETAGSF -a
endef

TAGS: FORCE
	$(call cmd,TAGS)


quiet_cmd_tags = MAKE   $@
define cmd_tags
	rm -f $@; \
	CTAGSF=`ctags --version | grep -i exuberant >/dev/null &&     \
                echo "-I __initdata,__exitdata,__acquires,__releases  \
                      -I EXPORT_SYMBOL,EXPORT_SYMBOL_GPL              \
                      --extra=+f --c-kinds=+px"`;                     \
                $(all-sources) | xargs ctags $$CTAGSF -a
endef

tags: FORCE
	$(call cmd,tags)


# Scripts to check various things for consistency
# ---------------------------------------------------------------------------

endif #ifeq ($(config-targets),1)
endif #ifeq ($(mixed-targets),1)




# Single targets
# ---------------------------------------------------------------------------
# Single targets are compatible with:
# - build whith mixed source and output
# - build with separate output dir 'make O=...'
# - external modules
#
#  target-dir => where to store outputfile
#  build-dir  => directory in kernel source tree to use

build-dir  = $(patsubst %/,%,$(dir $@))
target-dir = $(dir $@)


%.s: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.i: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.lst: %.c prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.s: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)
%.o: %.S prepare scripts FORCE
	$(Q)$(MAKE) $(build)=$(build-dir) $(target-dir)$(notdir $@)

# Modules
%/: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(NAUT_CONFIG_MODULES),1) \
	$(build)=$(build-dir)
%.ko: prepare scripts FORCE
	$(Q)$(MAKE) KBUILD_MODULES=$(if $(NAUT_CONFIG_MODULES),1)   \
	$(build)=$(build-dir) $(@:.ko=.o)
	$(Q)$(MAKE) -rR -f $(srctree)/scripts/Makefile.modpost

# FIXME Should go into a make.lib or something 
# ===========================================================================

quiet_cmd_rmdirs = $(if $(wildcard $(rm-dirs)),CLEAN   $(wildcard $(rm-dirs)))
      cmd_rmdirs = rm -rf $(rm-dirs)

quiet_cmd_rmfiles = $(if $(wildcard $(rm-files)),CLEAN   $(wildcard $(rm-files)))
      cmd_rmfiles = rm -f $(rm-files)


a_flags = -Wp,-MD,$(depfile) $(AFLAGS) $(AFLAGS_KERNEL) \
	  $(NOSTDINC_FLAGS) $(CPPFLAGS) \
	  $(modkern_aflags) $(EXTRA_AFLAGS) $(AFLAGS_$(*F).o)

quiet_cmd_as_o_S = AS      $@
cmd_as_o_S       = $(CC) $(a_flags) -c -o $@ $<

# read all saved command lines

targets := $(wildcard $(sort $(targets)))
cmd_files := $(wildcard .*.cmd $(foreach f,$(targets),$(dir $(f)).$(notdir $(f)).cmd))

ifneq ($(cmd_files),)
  $(cmd_files): ;	# Do not try to update included dependency files
  include $(cmd_files)
endif

# Shorthand for $(Q)$(MAKE) -f scripts/Makefile.clean obj=dir
# Usage:
# $(Q)$(MAKE) $(clean)=dir
clean := -f $(if $(KBUILD_SRC),$(srctree)/)scripts/Makefile.clean obj

endif	# skip-makefile

# Force the O variable for user and init_task (not set by kbuild?)
user init_task: O:=$(if $O,$O,$(objtree))
# Build Nautilus user-space libraries and example programs
user: FORCE
	if [ ! -d $O/$@ ]; then mkdir $O/$@; fi
	$(MAKE) \
		-C $(src)/$@ \
		O=$O/$@ \
		src=$(src)/$@ \
		all



PHONY += FORCE
FORCE:


# Declare the contents of the .PHONY variable as phony.  We keep that
# information in a variable se we can use it in if_changed and friends.
.PHONY: $(PHONY)
