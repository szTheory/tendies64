use core::alloc::Layout;

use linked_list_allocator::LockedHeap;

use crate::memory_map::MemoryMap;

#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::empty();

#[alloc_error_handler]
fn alloc_error_handler(_layout: Layout) -> ! {
    panic!("CPU profile heap exhausted")
}

pub fn init_allocator() {
    extern "C" {
        static __bss_end: u8;
        static __cpu_heap_limit: u8;
        static __cpu_stack_bottom: u8;
    }

    let heap_start = unsafe { &__bss_end as *const u8 as usize };
    let heap_end = unsafe { &__cpu_heap_limit as *const u8 as usize };
    let stack_bottom = unsafe { &__cpu_stack_bottom as *const u8 as usize };
    let stack_pointer: usize;
    unsafe {
        core::arch::asm!("move {}, $sp", out(reg) stack_pointer, options(nomem, nostack));
    }
    assert!(heap_start < heap_end, "CPU profile BSS overlaps the heap boundary");
    assert!(heap_end <= stack_bottom, "CPU profile heap overlaps the reserved stack");
    assert!(stack_pointer > stack_bottom, "CPU profile stack is outside the reserved region");
    assert_eq!(
        heap_end,
        MemoryMap::addr32_to_usize(0x8000_0000) + MemoryMap::HEAP_END
    );

    unsafe {
        ALLOCATOR
            .lock()
            .init(heap_start as *mut u8, heap_end - heap_start);
    }
}
