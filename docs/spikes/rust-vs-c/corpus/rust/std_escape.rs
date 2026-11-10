//! Rust-only probe, never a trigger: extern crate std re-enables the host.
#![no_std]
#![forbid(unsafe_code)]

extern crate std;

pub fn now() -> std::time::SystemTime {
    std::time::SystemTime::now()
}
