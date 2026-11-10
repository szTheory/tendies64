//! Mutable global. Twin of bad_mutable_global.c.
#![no_std]
#![forbid(unsafe_code)]

pub static mut GLOBAL: u32 = 7;

pub fn read() -> u32 {
    GLOBAL
}
