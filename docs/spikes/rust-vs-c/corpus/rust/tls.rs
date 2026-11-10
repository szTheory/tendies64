//! Thread-local storage. Twin of bad_tls.c.
#![no_std]
#![forbid(unsafe_code)]

std::thread_local! {
    static SEED_TLS: core::cell::Cell<u32> = const { core::cell::Cell::new(0) };
}

pub fn bump() -> u32 {
    SEED_TLS.with(|c| {
        c.set(c.get() + 1);
        c.get()
    })
}
