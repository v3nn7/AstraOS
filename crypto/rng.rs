#![no_std]
#![allow(clippy::identity_op)]
//! Kernel RNG: prefer hardware RDRAND, fallback to XORSHIFT128.
//! No std, no external deps. OSDev usage: early entropy (non-crypto strong).

use core::arch::asm;

/// Check if CPU supports RDRAND via CPUID.(EAX=1):ECX bit 30.
#[inline(always)]
fn is_rdrand_supported() -> bool {
    let mut ecx: u32 = 0;
    unsafe {
        asm!(
            "cpuid",
            inlateout("eax") 0x1 => _,
            lateout("ebx") _,
            lateout("ecx") ecx,
            lateout("edx") _,
        );
    }
    (ecx & (1 << 30)) != 0
}

/// Try a single RDRAND; returns Some(val) on success.
#[inline(always)]
fn rdrand_u64() -> Option<u64> {
    let mut ok: u8 = 0;
    let mut val: u64 = 0;
    unsafe {
        asm!(
            "rdrand {0}",
            "setc {1}",
            out(reg) val,
            out(reg_byte) ok,
            options(nomem, nostack),
        );
    }
    if ok != 0 { Some(val) } else { None }
}

// XORSHIFT128 fallback state (not cryptographically secure).
static mut XOR_STATE: [u32; 4] = [0x193a6754, 0xa8a7d469, 0x97830e05, 0x113ba7bb];

#[inline(always)]
fn xorshift128() -> u64 {
    unsafe {
        let mut t = XOR_STATE[3];
        let s = XOR_STATE[0];
        XOR_STATE[3] = XOR_STATE[2];
        XOR_STATE[2] = XOR_STATE[1];
        XOR_STATE[1] = s;
        t ^= t << 11;
        t ^= t >> 8;
        XOR_STATE[0] = t ^ s ^ (s >> 19);
        ((XOR_STATE[0] as u64) << 32) | (XOR_STATE[1] as u64)
    }
}

/// Get a u64 using RDRAND if available; fallback to XORSHIFT128 otherwise.
/// Suitable for IDs/seeds; not a replacement for CSPRNG without additional mixing.
pub fn rand_u64() -> u64 {
    if is_rdrand_supported() {
        if let Some(v) = rdrand_u64() {
            return v;
        }
    }
    xorshift128()
}

/// Fill `buf` with random bytes from `rand_u64` source.
pub fn rand_bytes(buf: &mut [u8]) {
    let mut i = 0;
    while i < buf.len() {
        let r = rand_u64();
        let mut j = 0;
        while j < 8 && i + j < buf.len() {
            buf[i + j] = (r >> (j * 8)) as u8;
            j += 1;
        }
        i += j;
    }
}
