#![no_std]
use core::ffi::{c_void,c_char};
#[panic_handler] fn panic(_: &core::panic::PanicInfo)->! { loop {} }
#[link(wasm_import_module="env")] unsafe extern "C" {
 #[link_name="pinInit"] fn pin_init(name:*const c_char,mode:u32)->i32;
 #[link_name="i2cInit"] fn i2c_init(config:*const Config)->u32;
 #[link_name="getSimNanos"] fn time_ns()->f64;
}
#[repr(C)] struct Config { user:*mut c_void, address:u32,scl:i32,sda:i32,connect:extern "C" fn(*mut c_void,u32,bool)->bool,read:extern "C" fn(*mut c_void)->u8,write:extern "C" fn(*mut c_void,u8)->bool,disconnect:extern "C" fn(*mut c_void), reserved:[u32;8] }
static mut REGISTER:u8=0;
static mut FIRST:bool=true;
extern "C" fn connect(_: *mut c_void,_:u32,read:bool)->bool {unsafe {if time_ns()<3_000_000_000.0 {return false} if !read {FIRST=true} true}}
extern "C" fn write(_: *mut c_void,value:u8)->bool {unsafe {if FIRST {REGISTER=value;FIRST=false} else {REGISTER=REGISTER.wrapping_add(1)} } true}
extern "C" fn read(_: *mut c_void)->u8 {unsafe {let v=match REGISTER {0x75=>0x68,0x3f=>0x40,_=>0};REGISTER=REGISTER.wrapping_add(1);v}}
extern "C" fn disconnect(_: *mut c_void) {}
#[unsafe(export_name="__wokwi_api_version_1")] pub extern "C" fn api_version()->i32 {1}
#[unsafe(export_name="chipInit")] pub extern "C" fn chip_init(){unsafe {let c=Config{user:core::ptr::null_mut(),address:0x68,scl:pin_init(c"SCL".as_ptr(),2),sda:pin_init(c"SDA".as_ptr(),2),connect,read,write,disconnect,reserved:[0;8]};i2c_init(&c);}}
