use std::sync::Arc;

use anyhow::anyhow;
use shuttle_persist::PersistInstance;
use tokio::sync::RwLock;

use super::{Color, LedArray};

const LED_ARRAY_KEY: &str = "leds";

#[derive(Clone)]
pub struct PersistedLedArray {
    led_array: Arc<RwLock<LedArray>>,
    persist: PersistInstance,
}

impl PersistedLedArray {
    pub fn new(persist: PersistInstance) -> Self {
        let led_array = Arc::new(RwLock::new(
            persist.load::<LedArray>(LED_ARRAY_KEY).unwrap_or_default(),
        ));

        Self { led_array, persist }
    }

    pub async fn get_leds(self) -> LedArray { *self.led_array.read().await }

    pub async fn get_led(self, index: usize) -> Option<Color> {
        self.led_array.read().await.get(index).copied()
    }

    pub async fn set_led(self, index: usize, color: Color) -> anyhow::Result<()> {
        let mut led_array = self.led_array.write().await;
        *led_array
            .get_mut(index)
            .ok_or(anyhow!("Index out of bounds"))? = color;
        self.persist
            .save(LED_ARRAY_KEY, *led_array)
            .map_err(anyhow::Error::new)
    }
}
