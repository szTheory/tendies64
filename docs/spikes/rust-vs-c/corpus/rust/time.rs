//! Host time. Twin of bad_time.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn now() -> std::time::SystemTime {
    std::time::SystemTime::now()
}
