pub mod channel;
pub mod constants;
pub mod engine;
pub mod ffi;
pub mod gain_sharing;
pub mod last_mic_hold;
pub mod level_detector;
pub mod math_utils;
pub mod noise_floor;
pub mod nom;
pub mod ring_buffer;
pub mod smoothing;

pub use constants::AUTOMIX_MAX_CHANNELS;
pub use engine::AutomixEngine;
