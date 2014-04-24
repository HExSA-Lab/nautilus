CC:=/home/kyle/opt/cross/bin/x86_64-elf-gcc
ISO:=nautilus.iso
CFLAGS:=    -O2 \
		   -std=gnu99 \
		   -Wall \
		   -Wmissing-prototypes \
		   -Wstrict-prototypes \
		   -Wno-unused-function \
		   -ffreestanding \
		   -fno-exceptions \
		   -mcmodel=large \
		   -mno-red-zone \
		   -mno-mmx \
		   -mno-sse \
		   -mno-sse2 \
		   -mno-sse3 \
		   -mno-3dnow 
LDFLAGS:=-nostdlib -z max-page-size=0x1000
LIBS:=-lgcc
INC:=-Iinclude
SRCDIR:=src
SRC:=$(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/*.S)
SFILES:=$(filter %.S, $(SRC))
CFILES:=$(filter %.c, $(SRC))
SOBJS:=$(SFILES:.S=.o)
COBJS:=$(CFILES:.c=.o)
OBJS:=$(SOBJS) $(COBJS)
DEPDIR:=.deps
DEPS:=$(wildcard $(DEPDIR)/*.d)
TARGET:=nautilus.bin
LDSCRIPT:=nautilus.ld

all: $(TARGET)

isoimage: $(ISO)

$(ISO): $(TARGET)
	cp $(TARGET) iso/boot
	grub-mkrescue -o $@ iso

$(TARGET): $(OBJS) $(LDSCRIPT)
	$(CC) -o $@ -T $(LDSCRIPT) $(OBJS) $(LDFLAGS) $(CFLAGS) $(LIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.S
	@if [ ! -d $(DEPDIR) ]; then mkdir $(DEPDIR); fi
	$(CC) $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$(@F).d $(INC) -c $< -o $@ 

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	@if [ ! -d $(DEPDIR) ]; then mkdir $(DEPDIR); fi
	$(CC) $(CFLAGS) -MMD -MP -MF $(DEPDIR)/$(@F).d $(INC) -c $< -o $@ 

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

distclean:
	rm -f $(OBJS) $(DEPS) $(TARGET) $(ISO) iso/boot/$(TARGET)

transfer: $(ISO)
	scp $(ISO) kch479@behemoth.cs.northwestern.edu:

.PHONY: clean all

-include $(OBJS:.o=.d)
