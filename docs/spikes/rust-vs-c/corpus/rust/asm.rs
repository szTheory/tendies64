//! Inline assembly. Twin of bad_asm.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn barrier(x: u32) -> u32 {
    core::arch::asm!("nop");
    x
}
