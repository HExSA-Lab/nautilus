
# Note, uses SDL
#(sleep 5 && vncviewer localhost:2) &

/usr/local/nautilus-toolchain/bin/qemu-system-x86_64 -cdrom nautilus.iso -m 2048 -smp 2   -serial stdio
