use crate::*;

// nomangle + pub extern "C" means standard C linkage and visibility
#[no_mangle]
pub extern "C" fn nk_rust_example(a: i32, b: i32) -> i32 {
    let _x: core::ffi::c_int = 5;
    unsafe {
        nk_vc_printf("Hello NK Rust Example\n".as_ptr() as *mut i8);
    }
    return a + b;
}
