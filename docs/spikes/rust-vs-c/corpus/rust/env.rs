//! Process environment. Twin of bad_env.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn home() -> bool {
    std::env::var("HOME").is_ok()
}
