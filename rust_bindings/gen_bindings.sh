#!/bin/bash

# for filename in ../include/nautilus/*; do
#     f=$(basename $filename)
#     f=${f%.*}
#     echo "Generating bindings for: " $filename
#     bindgen --ctypes-prefix ::libc --use-core --builtins $filename -o ./bindings/$f"_bindings.rs" -- -I../include/ -I../include/nautilus/ -include stdint.h -include ../include/nautilus/naut_types.h
# done

bindgen --ctypes-prefix core::ffi --use-core --builtins ../include/nautilus/vc.h -o ./bindings/vc_bindings.rs -- -I../include/ -I../include/nautilus/ -include stdint.h -include ../include/nautilus/naut_types.h
