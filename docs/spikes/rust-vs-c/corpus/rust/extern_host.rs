//! Extern host call. Twin of bad_extern_host.c.
#![no_std]
#![forbid(unsafe_code)]

extern "C" {
    fn host_poll() -> i32;
}

pub fn poll() -> i32 {
    host_poll()
}
