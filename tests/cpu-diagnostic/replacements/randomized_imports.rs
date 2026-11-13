use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::any::Any;
use core::arch::asm;

use arbitrary_int::prelude::*;
use oorandom::{Rand32, Rand64};

use crate::assembler::{Assembler, FR};
use crate::cop0::CauseException;
use crate::cop1::{fcsr, set_fcsr, FCSRRoundingMode, FCSR};
use crate::exception_handler::expect_exception;
use crate::tests::soft_asserts::soft_assert_eq;
use crate::tests::{Level, Test};
