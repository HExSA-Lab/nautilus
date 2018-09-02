// give us this feature to override?
#![feature(panic_handler, start)]

// cargo cult
#![feature(lang_items)]

// no stdlib 
#![no_std]

// avoid buildins - we want it to use our library
#![no_builtins]

// nomangle + pub extern "C" means standard C linkage and visibility
#[no_mangle]
pub extern "C" fn nk_rust_example(a: i32, b: i32) -> i32
{
    return a+b;
    //println!("Hello, world!");
}



// The following cruft is here to handle Rust->OS dependencies
// currently only one:  Rust needs to know how to panic
use core::panic::PanicInfo;

#[panic_handler]
#[no_mangle]
pub fn nk_rust_panic(_info: &PanicInfo) -> !
{
   // should call nk_panic here...
   loop { }
} 
 