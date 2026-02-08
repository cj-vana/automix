pub mod ffi;

/// Maximum number of channels supported.
pub const AUTOMIX_MAX_CHANNELS: usize = 32;

/// Core automix engine (stub for Phase 0).
pub struct AutomixEngine {
    num_channels: usize,
    sample_rate: f32,
}

impl AutomixEngine {
    pub fn new(num_channels: usize, sample_rate: f32) -> Self {
        Self {
            num_channels,
            sample_rate,
        }
    }

    pub fn version() -> &'static str {
        env!("CARGO_PKG_VERSION")
    }

    /// Stub process: passes audio through unmodified.
    pub unsafe fn process_raw(
        &mut self,
        _channel_ptrs: *const *mut f32,
        _num_channels: usize,
        _num_samples: usize,
    ) {
        // Phase 0: passthrough — audio buffers are left unmodified
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_engine_creation() {
        let engine = AutomixEngine::new(8, 48000.0);
        assert_eq!(engine.num_channels, 8);
        assert_eq!(engine.sample_rate, 48000.0);
    }

    #[test]
    fn test_version() {
        let version = AutomixEngine::version();
        assert_eq!(version, "0.1.0");
    }
}
