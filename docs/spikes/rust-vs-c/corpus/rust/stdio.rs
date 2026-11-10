//! Standard output. Twin of bad_include_stdio.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn say() {
    use std::io::Write;
    let _ = std::io::stdout().write_all(b"seed");
}
