//! Spike-only Rust twin of admission.c (docs/spikes/rust-vs-c).
//!
//! Same contract as admission.h, frozen in PREREG.md. The library form is
//! `no_std` and `forbid(unsafe_code)`. The `#[cfg(test)]` module needs std and
//! holds the contract table and the `differential` test, which regenerates the
//! exact input sequence of fuzz_admission.c and prints the same lines.
#![cfg_attr(not(test), no_std)]
#![forbid(unsafe_code)]

pub const MIN_LEN: usize = 4096;

pub const OK_Z64: u8 = 0;
pub const OK_V64: u8 = 1;
pub const OK_N64: u8 = 2;
pub const TOO_SMALL: u8 = 3;
pub const TOO_LARGE: u8 = 4;
pub const UNKNOWN_ORDER: u8 = 5;
pub const MISALIGNED: u8 = 6;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct AdmitInfo {
    /// 0 z64, 1 v64, 2 n64, -1 not admitted.
    pub order: i32,
    pub len: usize,
}

/// Check order: too small, too large, misaligned, then the byte order magic.
pub fn admit(buf: &[u8], limit: usize, out: &mut AdmitInfo) -> u8 {
    out.order = -1;
    out.len = 0;
    let len = buf.len();
    if len < MIN_LEN {
        return TOO_SMALL;
    }
    if len > limit {
        return TOO_LARGE;
    }
    if len % 4 != 0 {
        return MISALIGNED;
    }
    let order = match [buf[0], buf[1], buf[2], buf[3]] {
        [0x80, 0x37, 0x12, 0x40] => 0,
        [0x37, 0x80, 0x40, 0x12] => 1,
        [0x40, 0x12, 0x37, 0x80] => 2,
        _ => return UNKNOWN_ORDER,
    };
    out.order = order;
    out.len = len;
    order as u8
}

#[cfg(test)]
mod tests {
    use super::*;

    const LIMIT: usize = 67_108_864;
    const SEED: u64 = 0x2545_F491_4F6C_DD1D;
    const FNV_OFFSET: u64 = 0xcbf2_9ce4_8422_2325;
    const FNV_PRIME: u64 = 0x0000_0100_0000_01b3;
    const DEFAULT_MUTATIONS: u64 = 2_000_000;
    const MAGIC: [[u8; 4]; 4] = [
        [0x80, 0x37, 0x12, 0x40],
        [0x37, 0x80, 0x40, 0x12],
        [0x40, 0x12, 0x37, 0x80],
        [0x00, 0x00, 0x00, 0x00],
    ];

    struct Rng(u64);

    impl Rng {
        fn next(&mut self) -> u64 {
            let mut x = self.0;
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            self.0 = x;
            x
        }
    }

    struct Tally {
        counts: [u64; 7],
        fnv: u64,
        inputs: u64,
    }

    impl Tally {
        fn run_one(&mut self, buf: &[u8], limit: usize) -> u8 {
            let mut info = AdmitInfo { order: -2, len: 0 };
            let rc = admit(buf, limit, &mut info);
            assert!(rc <= 6);
            if rc <= 2 {
                assert_eq!(info.order, rc as i32);
                assert_eq!(info.len, buf.len());
            } else {
                assert_eq!(info.order, -1);
                assert_eq!(info.len, 0);
            }
            self.counts[rc as usize] += 1;
            self.fnv ^= rc as u64;
            self.fnv = self.fnv.wrapping_mul(FNV_PRIME);
            self.inputs += 1;
            rc
        }
    }

    /// Header magic, then 32 random bytes at offsets 4..35, rest zero.
    fn make_input(rng: &mut Rng, len: usize, m: usize) -> Vec<u8> {
        let mut b = vec![0u8; len];
        if len >= 4 {
            b[..4].copy_from_slice(&MAGIC[m]);
        }
        let mut off = 4usize;
        while off < len && off < 36 {
            let v = rng.next();
            let mut k = 0usize;
            while k < 8 && off + k < len {
                b[off + k] = (v >> (8 * k)) as u8;
                k += 1;
            }
            off += 8;
        }
        b
    }

    #[test]
    fn table() {
        let cases: [(usize, usize, usize, u8); 10] = [
            (4096, 0, LIMIT, 0),
            (4096, 1, LIMIT, 1),
            (4096, 2, LIMIT, 2),
            (4096, 3, LIMIT, 5),
            (4095, 0, LIMIT, 3),
            (4097, 0, LIMIT, 6),
            (8192, 0, 4096, 4),
            (0, 0, LIMIT, 3),
            (4100, 2, LIMIT, 2),
            (LIMIT + 4, 0, LIMIT, 4),
        ];
        for (i, (len, m, limit, expect)) in cases.iter().enumerate() {
            let mut b = vec![0u8; *len];
            if *len >= 4 {
                b[..4].copy_from_slice(&MAGIC[*m]);
            }
            let mut info = AdmitInfo { order: -2, len: 0 };
            let rc = admit(&b, *limit, &mut info);
            assert_eq!(rc, *expect, "table case {i}");
        }
    }

    #[test]
    fn differential() {
        // Harness controls may set T64S_MUTATIONS; the registered run does not.
        let mutations: u64 = std::env::var("T64S_MUTATIONS")
            .ok()
            .and_then(|v| v.parse().ok())
            .unwrap_or(DEFAULT_MUTATIONS);
        let sizes: [usize; 10] = [0, 1, 3, 4, 4031, 4032, 4033, LIMIT - 1, LIMIT, LIMIT + 1];
        let mut rng = Rng(SEED);
        let mut t = Tally { counts: [0; 7], fnv: FNV_OFFSET, inputs: 0 };
        // The first line must start on a fresh line under --nocapture.
        println!();
        for size in sizes.iter() {
            let mut b = vec![0u8; *size];
            let mut codes = [0u8; 4];
            for m in 0..4usize {
                if *size >= 4 {
                    b[..4].copy_from_slice(&MAGIC[m]);
                }
                codes[m] = t.run_one(&b, LIMIT);
            }
            println!("boundary {}: {}{}{}{}", size, codes[0], codes[1], codes[2], codes[3]);
        }
        for _ in 0..mutations {
            let r = rng.next();
            let mut limit = LIMIT;
            let b: Vec<u8>;
            match r & 3 {
                0 => {
                    let len = (4096 + 4 * ((r >> 2) % 16)) as usize;
                    let m = ((r >> 8) % 3) as usize;
                    let mut x = make_input(&mut rng, len, m);
                    let nf = 1 + (r >> 16) % 4;
                    for _ in 0..nf {
                        let v = rng.next();
                        x[(v % 8) as usize] ^= ((v >> 8) | 1) as u8;
                    }
                    b = x;
                }
                1 => {
                    let len = ((r >> 2) % 4100) as usize;
                    let m = ((r >> 20) % 3) as usize;
                    b = make_input(&mut rng, len, m);
                }
                2 => {
                    let len = (4096 + (r >> 2) % 4200) as usize;
                    let m = ((r >> 20) % 3) as usize;
                    b = make_input(&mut rng, len, m);
                    let v = rng.next();
                    limit = (4096 + v % 4200) as usize;
                }
                _ => {
                    let len = ((r >> 2) % 16384) as usize;
                    let m = ((r >> 20) % 4) as usize;
                    b = make_input(&mut rng, len, m);
                }
            }
            t.run_one(&b, limit);
        }
        println!("mutations: {}", mutations);
        println!("inputs: {}", t.inputs);
        println!(
            "outcomes: {} {} {} {} {} {} {}",
            t.counts[0], t.counts[1], t.counts[2], t.counts[3], t.counts[4], t.counts[5], t.counts[6]
        );
        println!("fnv1a64: {:016x}", t.fnv);
        println!("info-consistency: ok");
    }
}
