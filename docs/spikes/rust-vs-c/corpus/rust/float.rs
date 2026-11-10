//! Floating point. Twin of bad_float.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn scale(x: f32) -> f32 {
    x * 0.5
}
