CC=/home/kyle/opt/cross/bin/x86_64-elf-gcc
ISO=nautilus.iso
CFLAGS=    -O2 \
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
LDFLAGS=-nostdlib -z max-page-size=0x1000
LIBS=-lgcc
INC=-Iinclude
OBJS=boot.o cga.o string.o doprnt.o printk.o nautilus.o
TARGET=nautilus.bin
LDSCRIPT=nautilus.ld

all: $(TARGET)

isoimage: $(ISO)

$(ISO): $(TARGET)
	cp $(TARGET) iso/boot
	grub-mkrescue -o $@ iso

$(TARGET): $(OBJS) $(LDSCRIPT)
	$(CC) -T $(LDSCRIPT) $(OBJS) -o $@ $(LDFLAGS) $(CFLAGS) $(LIBS)

%.o: %.S
	$(CC) $(CFLAGS) $(INC) -c $< -o $@ 

%.o: %.c
	$(CC) $(CFLAGS) $(INC) -c $< -o $@ 

clean:
	rm -f $(OBJS) $(TARGET)

distclean:
	rm -f $(OBJS) $(TARGET) $(ISO) iso/boot/$(TARGET)

test:
	echo $(OBJS)

.PHONY: clean
