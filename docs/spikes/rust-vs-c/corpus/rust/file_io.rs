//! File I/O. Twin of bad_file_io.c.
#![no_std]
#![forbid(unsafe_code)]

pub fn open() -> bool {
    std::fs::File::open("seed.bin").is_ok()
}
