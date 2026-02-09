pub mod constants;
pub mod math_utils;
pub mod gain_sharing;
pub mod nom;
pub mod ring_buffer;
pub mod level_detector;
pub mod smoothing;
pub mod noise_floor;
pub mod last_mic_hold;
pub mod channel;
pub mod engine;
pub mod ffi;

pub use constants::AUTOMIX_MAX_CHANNELS;
pub use engine::AutomixEngine;
