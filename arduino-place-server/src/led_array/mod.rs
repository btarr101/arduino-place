use std::ops::{Deref, DerefMut};

use derive_more::derive::Display;
use serde::{Deserialize, Serialize};

pub const LED_COUNT: usize = 100;

pub mod persisted;

#[derive(Clone, Copy, Serialize, Deserialize, Default, Debug, Display)]
#[display("({}, {}, {})", red, green, blue)]
pub struct Color {
    pub red: u8,
    pub green: u8,
    pub blue: u8,
}

#[derive(Clone, Copy)]
pub struct LedArray([Color; LED_COUNT]);

impl From<LedArray> for Vec<u8> {
    fn from(val: LedArray) -> Self {
        val.into_iter()
            .flat_map(|color| [color.red, color.green, color.blue])
            .collect()
    }
}

impl Default for LedArray {
    fn default() -> Self { Self([Color::default(); LED_COUNT]) }
}

impl Deref for LedArray {
    type Target = [Color; LED_COUNT];

    fn deref(&self) -> &Self::Target { &self.0 }
}

impl DerefMut for LedArray {
    fn deref_mut(&mut self) -> &mut Self::Target { &mut self.0 }
}

impl Serialize for LedArray {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        self.to_vec().serialize(serializer)
    }
}

impl<'de> Deserialize<'de> for LedArray {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        let vec = <Vec<Color>>::deserialize(deserializer)?;
        let array: [Color; LED_COUNT] = vec
            .try_into()
            .map_err(|vec| serde::de::Error::custom(format!("{:?}", vec)))?;
        Ok(Self(array))
    }
}
