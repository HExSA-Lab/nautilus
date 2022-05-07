#
# Rust is apparently incapable of just producing a bloody .o file,
# so we will get it to produce a static library (a .a file) as
# the next best thing.   Theoretically, this file is supposed to be
# self-contained as well.
#
# Now, we are not invoking or using anything exported from the .a file
# here, because our overall build process assumes we produce .os.
# Normally this would mean the linker would just discard the .a content
# at this point.

obj-y := glue.o libparport.a

#
# Force this step to happen all the time.  We need to use
# Cargo to do the Rust build because, of course, you must use their
# build environment...
#
#
.PHONY:  src/rust/parport/libparport.a clean
src/rust/parport/libparport.a:
	(cd src/rust/parport && cargo xbuild --release --target x86_64-nautilus-core-kernel)
	(cd src/rust/parport && cp target/x86_64-nautilus-core-kernel/release/libparport.a .)

#
#
# Does NOT work
#
clean:
	(cd src/rust/parport && cargo clean)
