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

obj-y := glue.o libnautilus_rust.a

#
# Force this step to happen all the time.  We need to use
# Cargo to do the Rust build because, of course, you must use their
# build environment...
#
#
.PHONY:  src/rust/libnautilus_rust.a clean
src/rust/libnautilus_rust.a:
	(cd src/rust && cargo xbuild --release --target x86_64-nautilus-core-kernel.json)
	(cd src/rust && cp target/x86_64-nautilus-core-kernel/release/libnautilus_rust.a .)

#
#
# Does NOT work
#
clean:
	(cd src/rust && cargo clean)
