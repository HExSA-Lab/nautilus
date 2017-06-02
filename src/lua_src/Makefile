
obj-y += lmem.o\
	 lstate.o \
	 ldebug.o \
	 ldo.o \
	 lgc.o \
	 ltable.o \
	 lstring.o \
	 ltm.o \
	 llex.o \
	 lapi.o \
	 lfunc.o \
	 lbitlib.o \
	 lbaselib.o \
	 lauxlib.o \
	 ldblib.o \
	 lcorolib.o \
	 lcode.o \
	 linit.o \
 	 lctype.o \
	 ldump.o \
	 lopcodes.o \
	 liolib.o \
	 lmathlib.o \
         loadlib.o \
	 lobject.o \
	 lvm.o \
	 loslib.o \
	 lparser.o \
	 lstrlib.o \
 	 ltablib.o \
	 lua.o \
	 lzio.o  \
	 lundump.o \

ifeq ($(LUA_BUILD_FLAG),1)
CFLAGS+= -DLUA_BUILD_FLAG
obj-y += lnautlib.o
endif
