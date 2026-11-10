//! Load-time constructor. Twin of bad_ctor.c.
#![no_std]
#![forbid(unsafe_code)]

extern "C" fn seed_ctor() {}

#[used]
#[link_section = ".init_array"]
static SEED_INIT: extern "C" fn() = seed_ctor;
