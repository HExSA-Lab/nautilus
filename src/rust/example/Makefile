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

obj-y := glue.o libexample.a 

#
# Force this step to happen all the time.  We need to use
# Cargo to do the Rust build because, of course, you must use their
# build environment... 
#
#
.PHONY:  src/rust/example/libexample.a clean
src/rust/example/libexample.a:
	(cd src/rust/example &&	cargo xbuild --release --target x86_64-nautilus-core-kernel)
	(cd src/rust/example && cp target/x86_64-nautilus-core-kernel/release/libexample.a .)

#
#
# Does NOT work
#
clean:
	(cd src/rust/example && cargo clean)



