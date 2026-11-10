//! Control twin of good.c: allowed constructs only.
#![no_std]
#![forbid(unsafe_code)]

pub struct SeedState {
    pub value: u32,
    pub floating_point_mode: u32,
    pub bytes: [u8; 8],
}

const TABLE: [u32; 4] = [1, 2, 3, 4];
const NOTE: &str = "float time malloc 0.5 __TIME__";

fn mix(a: u32, b: u32) -> u32 {
    (a << 3) ^ b
}

pub fn good(st: &mut SeedState, src: &[u8]) -> u32 {
    let n = src.len().min(8);
    st.bytes = [0; 8];
    st.bytes[..n].copy_from_slice(&src[..n]);
    st.floating_point_mode = NOTE.as_bytes()[0] as u32 + 34;
    st.value = mix(TABLE[(st.bytes[0] & 3) as usize], st.floating_point_mode);
    st.value
}
