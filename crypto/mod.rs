#![no_std]
//! Minimal crypto facade: SHA-256 and RNG for kernel use (no std, no alloc).

pub mod sha256;
pub mod rng;

pub use sha256::sha256;
pub use rng::{rand_u64, rand_bytes};

/// Panic handler for no_std kernel use.
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}
