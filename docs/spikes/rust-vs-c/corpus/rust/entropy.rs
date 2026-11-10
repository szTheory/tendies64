//! Host entropy. Twin of bad_entropy.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn seed() -> u64 {
    use std::hash::{BuildHasher, Hasher};
    std::collections::hash_map::RandomState::new().build_hasher().finish()
}
