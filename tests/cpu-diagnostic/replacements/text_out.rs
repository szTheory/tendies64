pub fn text_out(_text: &str) {}

pub fn protocol_out(record: &str) {
    crate::isviewer::text_out(record);
}
