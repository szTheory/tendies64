//! Threads. Twin of bad_thread.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn spawn() {
    let _ = std::thread::spawn(|| {});
}
