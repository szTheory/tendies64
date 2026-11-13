use alloc::boxed::Box;
use alloc::vec::Vec;
use core::any::Any;

mod address_error_exception;
mod arithmetic;
mod cache;
mod cop0;
mod cop1;
mod cop_unusable;
mod endian_re;
mod exception_instructions;
mod jumps;
mod overflow_exception;
mod privilege;
mod soft_asserts;
mod testlist;
mod tlb;
mod tlb64;
mod traps;

#[derive(Copy, Clone, Eq, PartialEq)]
#[repr(u8)]
pub enum Level {
    BasicFunctionality = 0,
    RarelyUsed = 1,
    Weird = 2,
    RDPBasic = 3,
    RDPPrecise = 4,
    Timing = 5,
    COP0Hazard = 6,
    Cycle = 7,
    PoorlyUnderstoodQuirk = 8,
    StressTest = 9,
    _COUNT = 10,
}

pub trait Test {
    fn name(&self) -> &str;
    fn level(&self) -> Level;
    fn values(&self) -> Vec<Box<dyn Any>>;
    fn run(&self, value: &Box<dyn Any>) -> Result<(), alloc::string::String>;
}

fn boxed_values<T: Copy + 'static>(table: &'static [T]) -> Vec<Box<dyn Any>> {
    table
        .iter()
        .map(|value| Box::new(*value) as Box<dyn Any>)
        .collect()
}
