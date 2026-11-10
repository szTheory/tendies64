//! Mutable static. Twin of bad_mutable_static.c.
#![no_std]
#![forbid(unsafe_code)]

static mut COUNTER: u32 = 0;

pub fn bump() -> u32 {
    COUNTER += 1;
    COUNTER
}
