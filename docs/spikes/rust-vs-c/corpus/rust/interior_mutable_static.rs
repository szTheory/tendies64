//! Atomic (interior-mutable) static. Twin of bad_atomic.c.
#![no_std]
#![forbid(unsafe_code)]

use core::sync::atomic::{AtomicU32, Ordering};

static COUNTER: AtomicU32 = AtomicU32::new(0);

pub fn bump() -> u32 {
    COUNTER.fetch_add(1, Ordering::Relaxed)
}
