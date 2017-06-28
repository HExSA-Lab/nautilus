# 
#  Copyright (c) 2017 Peter Dinda 
#
# Framework code for building "user" executables for Nautilus
#

NAUTILUS_DIR = /home/pdinda/test/rt/nautilus


TARGET = hello.exe

USER_OBJ = hello.o

LD = ld

# Flags must be compatible with the NK kernel flags
CFLAGS = -O2 \
	 -fno-omit-frame-pointer \
	 -ffreestanding \
	 -fno-stack-protector \
	 -fno-strict-aliasing \
         -fno-strict-overflow \
	 -fno-delete-null-pointer-checks \
         -fno-exceptions \
	 -mno-red-zone \
	 -mcmodel=large \
         -fno-common \
         -Wstrict-overflow=5

# For C++ add
#         -fno-rtti \

# Add these for more recent compilers to avoid having
# the compiler insert surprise ud2 instructions for you should
# you derefence address zero
#
#                           -fno-isolate-erroneous-paths-attribute \
#                           -fno-isolate-erroneous-paths-dereference \
#
#


CC = gcc -static -fPIC -Wall -I$(NAUTILUS_DIR)/include -DNAUTILUS_EXE

all: $(TARGET)

$(TARGET:.exe=.o) : $(TARGET:.exe=.c) 
	$(CC) $(CFLAGS) -c $(TARGET:.exe=.c)

framework_low_level.o: framework_low_level.S
	$(CC) $(CFLAGS) -c framework_low_level.S

framework.o : framework.c
	$(CC) $(CFLAGS) -c framework.c

$(TARGET) : $(TARGET:.exe=.o) framework.o framework_low_level.o framework.lds
	ld -T framework.lds -static -dp -z max-page-size=0x1000 $(TARGET:.exe=.o) framework.o framework_low_level.o -o $(TARGET)

clean: 
	rm -f $(TARGET) *.o

