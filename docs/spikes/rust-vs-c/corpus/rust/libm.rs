//! libm call. Twin of bad_libm.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn s(x: f64) -> f64 {
    x.sin()
}
