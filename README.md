    _   __               __   _  __                
   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    
  /  |/ // __ `// / / // __// // // / / // ___/ 
 / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     
/_/ |_/ \__,_/ \__,_/ \__//_//_/ \__,_//____/


# Nautilus

Nautilus is a small kernel aimed at parallel runtime co-design


# Prerequisites
- gcc cross compiler (more on this to come)
- grub version >= 2.02
- xorriso (for CD burning)
- QEMU recommended if you don't want to run on raw hardware

# Building
Run `make` to build the binary image. To make a bootable CD-ROM, 
run `make isoimage`. If you see weird errors, chances are there
is something wrong with the `grub-mkrescue`. Make sure grub-mkrescue 
knows where it's libraries are, especially if you've installed the
latest GRUB from source. Use `grub-mkrescue -d`.


# Running under QEMU
Recommended:

`qemu-system-x86_64 -cdrom nautilus.iso -m 2048 -smp 2 -vnc :0 -monitor stdio`

# Debugging
Nautilus supports debugging over the serial port. This is useful if you want to
debug a physical machine remotely. All prints after the serial port has been
initialized will be redirected to COM1.

Hitting `k` should reboot the machine if it's not completely locked up.


Kyle C. Hale (c) 2014
Northwestern University
