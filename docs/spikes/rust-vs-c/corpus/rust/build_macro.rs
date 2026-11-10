//! Build-time nondeterminism macro. Twin of bad_time_macro.c.
#![no_std]
#![forbid(unsafe_code)]

pub const HOME_AT_BUILD: &str = env!("HOME");
