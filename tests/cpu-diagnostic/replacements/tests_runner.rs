fn park() -> ! {
    loop {}
}

pub fn run() -> ! {
    let selector = crate::T64_CPU_SELECTOR;
    let registration_index = (selector >> 16) as usize;
    let value_index = (selector & 0xffff) as usize;
    let tests = testlist::tests();
    let (selected_index, test) = match tests
        .into_iter()
        .find(|(index, _)| *index == registration_index)
    {
        Some(selected) => selected,
        None => park(),
    };
    let mut values = test.values();
    let value: Box<dyn core::any::Any> = if values.is_empty() {
        if value_index != 0 {
            park();
        }
        Box::new(())
    } else {
        if value_index >= values.len() {
            park();
        }
        values.remove(value_index)
    };

    unsafe { crate::cop0::set_status(crate::cop0::Status::DEFAULT) };
    crate::cop1::set_fcsr(crate::cop1::FCSR::DEFAULT);
    let result = test.run(&value);
    unsafe { crate::cop0::set_status(crate::cop0::Status::DEFAULT) };
    crate::cop1::set_fcsr(crate::cop1::FCSR::DEFAULT);
    let failed = crate::exception_handler::drain_seen_exception().is_some() || result.is_err();
    let status = if failed { "fail" } else { "pass" };
    crate::text_out::protocol_out(&alloc::format!(
        "{{\"records\":[{{\"event\":\"start\",\"registration_index\":{},\"value_index\":{}}},{{\"event\":\"outcome\",\"status\":\"{}\"}},{{\"complete\":true,\"event\":\"done\",\"record_count\":\"3\"}}],\"schema\":\"t64.cpu-run\",\"schema_version\":1}}\n",
        selected_index, value_index, status
    ));
    park()
}
