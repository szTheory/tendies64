#[doc(hidden)]
pub fn _print(_args: core::fmt::Arguments<'_>) {}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {{
        let _ = core::format_args!($($arg)*);
    }};
}

#[macro_export]
macro_rules! println {
    () => {};
    ($($arg:tt)*) => {{
        let _ = core::format_args!($($arg)*);
    }};
}
