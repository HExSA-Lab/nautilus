#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]

// unstable feature core::ffi
#![feature(core_ffi_c)]

// cargo cult
#![feature(lang_items)]

// no stdlib
#![no_std]

// avoid buildins - we want it to use our library
#![no_builtins]

// import vc bindings
include!("../../../../rust_bindings/bindings/vc_bindings.rs");


// nomangle + pub extern "C" means standard C linkage and visibility
#[no_mangle]
pub extern "C" fn parport(a: i32, b: i32) -> i32 {

    let x: core::ffi::c_int = 5;
    unsafe {
        nk_vc_printf("parport\n".as_ptr() as *mut i8);
    }
    return a+b;
}


// The following cruft is here to handle Rust->OS dependencies
// currently only one:  Rust needs to know how to panic
use core::panic::PanicInfo;

#[panic_handler]
#[no_mangle]
pub fn parport_panic(_info: &PanicInfo) -> ! {
   // should call nk_panic here...
   loop { }
}
