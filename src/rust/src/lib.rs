#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![feature(core_ffi_c)] // unstable feature core::ffi
#![feature(lang_items)] // cargo cult
#![no_std] // no stdlib
#![no_builtins] // avoid buildins - we want it to use our library
include!("../../../rust_bindings/bindings/vc_bindings.rs"); // import vc bindings

mod example;
mod parport;

// The following cruft is here to handle Rust->OS dependencies
// currently only one:  Rust needs to know how to panic
use core::panic::PanicInfo;

#[panic_handler]
#[no_mangle]
pub fn nk_rust_panic(_info: &PanicInfo) -> ! {
    // should call nk_panic here...
    loop {}
}
