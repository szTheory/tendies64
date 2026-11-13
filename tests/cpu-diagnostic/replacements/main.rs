#![no_std]
#![feature(alloc_error_handler)]
#![feature(asm_experimental_arch)]
#![feature(step_trait)]
#![deny(unsafe_op_in_unsafe_fn)]
#![allow(rustdoc::private_intra_doc_links)]
#![no_main]
#![deny(unused_must_use)]

extern crate alloc;

use core::arch::{asm, naked_asm};
use core::ptr;

use crate::cop1::{set_fcsr, FCSR};
use crate::memory_map::MemoryMap;

mod allocator;
mod assembler;
mod cop0;
mod cop1;
mod exception_handler;
mod graphics;
mod isviewer;
mod math;
mod memory_map;
mod mi;
mod panic;
mod pi;
mod print;
mod rsp;
mod tests;
mod text_out;
mod uncached_memory;

const CPU_PROFILE_RDRAM_BYTES: usize = 4 * 1024 * 1024;
const CPU_PROFILE_RDRAM_BASE: usize = 0xffff_ffff_8000_0000;
const CPU_PROFILE_RDRAM_END: usize = CPU_PROFILE_RDRAM_BASE + CPU_PROFILE_RDRAM_BYTES;
const CPU_PROFILE_LOAD_ADDRESS: u32 = 0x8000_0400;
const CPU_PROFILE_APP_ROM_OFFSET: u32 = 0x1000;
const CPU_PROFILE_ELF_HEADER_OFFSET: usize =
    (CPU_PROFILE_APP_ROM_OFFSET - (CPU_PROFILE_LOAD_ADDRESS - 0x8000_0000)) as usize;
const CPU_PROFILE_STACK_TOP: u32 = 0x8040_0000;

#[used]
#[no_mangle]
#[link_section = ".t64_selector"]
pub(crate) static T64_CPU_SELECTOR: u32 = 0;

#[unsafe(naked)]
#[no_mangle]
#[link_section = ".t64_entrypoint"]
extern "C" fn entrypoint() -> ! {
    naked_asm!(
        ".set noat",
        ".set noreorder",
        "lui $29, 0x8040",
        "daddiu $29, $29, -16",
        "j {rust_entrypoint}",
        "nop",
        rust_entrypoint = sym rust_entrypoint,
    );
}

extern "C" fn rust_entrypoint() -> ! {
    extern "C" {
        static mut __bss_start: u8;
        static __bss_end: u8;
        static __cpu_heap_limit: u8;
        static __cpu_stack_bottom: u8;
        static __cpu_stack_top: u8;
    }

    let bss_start = unsafe { ptr::addr_of_mut!(__bss_start) };
    let bss_end = unsafe { ptr::addr_of!(__bss_end) };
    let bss_start_address = bss_start as usize;
    let bss_end_address = bss_end as usize;
    let heap_limit = unsafe { ptr::addr_of!(__cpu_heap_limit) as usize };
    let stack_bottom = unsafe { ptr::addr_of!(__cpu_stack_bottom) as usize };
    let stack_top = unsafe { ptr::addr_of!(__cpu_stack_top) as usize };
    if bss_start_address < CPU_PROFILE_RDRAM_BASE
        || bss_start_address > bss_end_address
        || bss_end_address > heap_limit
        || bss_end_address > CPU_PROFILE_RDRAM_END
        || heap_limit > stack_bottom
        || stack_bottom < CPU_PROFILE_RDRAM_BASE
        || stack_bottom > stack_top
        || stack_top > CPU_PROFILE_RDRAM_END
    {
        crate::tests::park();
    }
    let bss_length = bss_end_address - bss_start_address;
    unsafe { ptr::write_bytes(bss_start, 0, bss_length) };

    unsafe {
        asm!("move $26, $zero", "move $27, $zero", options(nomem, nostack));
    }
    assert_eq!(CPU_PROFILE_LOAD_ADDRESS, 0x8000_0400);
    assert_eq!(CPU_PROFILE_STACK_TOP, 0x8040_0000);
    MemoryMap::init(CPU_PROFILE_RDRAM_BYTES, CPU_PROFILE_ELF_HEADER_OFFSET);
    set_fcsr(
        FCSR::DEFAULT
            .with_flush_denorm_to_zero(true)
            .with_enable_invalid_operation(true),
    );
    mi::clear_interrupt_mask();
    allocator::init_allocator();
    exception_handler::install_exception_handlers();
    tests::run()
}
