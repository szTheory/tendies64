use alloc::format;
use core::arch::naked_asm;
use core::ops::{Deref, DerefMut};

use spinning_top::Spinlock;

use super::cop0;
use crate::cop0::{Cause, CauseException, Context, XContext};
use crate::cop1::FCSR;
use crate::memory_map::MemoryMap;
