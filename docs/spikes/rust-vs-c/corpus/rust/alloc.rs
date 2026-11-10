//! Heap allocation. Twin of bad_malloc.c.
#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

use alloc::boxed::Box;

pub fn make() -> Box<u32> {
    Box::new(1)
}
