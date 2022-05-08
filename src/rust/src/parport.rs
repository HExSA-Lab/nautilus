use crate::*;

// nomangle + pub extern "C" means standard C linkage and visibility
#[no_mangle]
pub extern "C" fn nk_rust_parport(_a: i32, _b: i32) -> i32 {
    let _x: core::ffi::c_int = 10;
    unsafe {
        nk_vc_printf("Hello NK Rust Parallel Port\n".as_ptr() as *mut i8);
    }
    return 10;
}
