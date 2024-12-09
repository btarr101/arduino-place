use led_array::Color;

pub mod led_array;
pub mod ws;

#[derive(Clone, Copy)]
pub enum BroadcastMessage {
    LedUpdated { index: usize, color: Color },
}
