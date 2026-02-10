use crate::constants::{
    DEFAULT_ATTACK_MS, DEFAULT_RELEASE_MS, DEFAULT_RMS_WINDOW_MS, DEFAULT_WEIGHT,
};
use crate::level_detector::LevelDetector;
use crate::math_utils::linear_to_db;
use crate::noise_floor::NoiseFloorTracker;
use crate::smoothing::OnePoleSmoother;

/// Per-channel user-controllable parameters.
#[derive(Debug, Clone)]
pub struct ChannelParams {
    pub weight: f64,
    pub muted: bool,
    pub soloed: bool,
    pub bypassed: bool,
}

impl Default for ChannelParams {
    fn default() -> Self {
        Self {
            weight: DEFAULT_WEIGHT,
            muted: false,
            soloed: false,
            bypassed: false,
        }
    }
}

impl ChannelParams {
    /// Determine if this channel participates in gain-sharing.
    ///
    /// A channel participates if:
    /// - Not muted
    /// - Not bypassed
    /// - Either no channel is soloed (`any_solo_active` is false),
    ///   or this channel is soloed
    pub fn is_participating(&self, any_solo_active: bool) -> bool {
        if self.muted || self.bypassed {
            return false;
        }
        if any_solo_active && !self.soloed {
            return false;
        }
        true
    }
}

/// Metering snapshot for a single channel.
#[derive(Debug, Clone, Copy, Default)]
pub struct ChannelMetering {
    pub input_rms_db: f64,
    pub gain_db: f64,
    pub output_rms_db: f64,
    pub noise_floor_db: f64,
    pub is_active: bool,
}

/// Per-channel state combining parameters, detectors, and metering.
pub struct Channel {
    pub index: usize,
    pub params: ChannelParams,
    pub level_detector: LevelDetector,
    pub noise_floor: NoiseFloorTracker,
    pub gain_smoother: OnePoleSmoother,
    pub raw_gain: f64,
    pub smoothed_gain: f64,
    pub is_active: bool,
    pub metering: ChannelMetering,
}

impl Channel {
    pub fn new(index: usize, sample_rate: f64) -> Self {
        Self {
            index,
            params: ChannelParams::default(),
            level_detector: LevelDetector::new(DEFAULT_RMS_WINDOW_MS, sample_rate),
            noise_floor: NoiseFloorTracker::new(sample_rate),
            gain_smoother: OnePoleSmoother::from_ms(
                DEFAULT_ATTACK_MS,
                DEFAULT_RELEASE_MS,
                sample_rate,
            ),
            raw_gain: 0.0,
            smoothed_gain: 0.0,
            is_active: false,
            metering: ChannelMetering::default(),
        }
    }

    /// Update metering snapshot from current state.
    pub fn update_metering(&mut self, input_rms: f64) {
        self.metering.input_rms_db = linear_to_db(input_rms);
        self.metering.gain_db = linear_to_db(self.smoothed_gain);
        self.metering.output_rms_db = linear_to_db(input_rms * self.smoothed_gain);
        self.metering.noise_floor_db = self.noise_floor.floor_db();
        self.metering.is_active = self.is_active;
    }

    /// Reset channel state (preserves params).
    pub fn reset(&mut self, sample_rate: f64) {
        self.level_detector.reset();
        self.noise_floor.reset(sample_rate);
        self.gain_smoother.reset();
        self.raw_gain = 0.0;
        self.smoothed_gain = 0.0;
        self.is_active = false;
        self.metering = ChannelMetering::default();
    }

    /// Update smoothing coefficients.
    pub fn set_smoothing(&mut self, attack_ms: f64, release_ms: f64, sample_rate: f64) {
        self.gain_smoother
            .set_coefficients(attack_ms, release_ms, sample_rate);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_params() {
        let p = ChannelParams::default();
        assert_eq!(p.weight, DEFAULT_WEIGHT);
        assert!(!p.muted);
        assert!(!p.soloed);
        assert!(!p.bypassed);
    }

    #[test]
    fn participating_normal() {
        let p = ChannelParams::default();
        assert!(p.is_participating(false));
    }

    #[test]
    fn muted_not_participating() {
        let p = ChannelParams {
            muted: true,
            ..Default::default()
        };
        assert!(!p.is_participating(false));
        assert!(!p.is_participating(true));
    }

    #[test]
    fn bypassed_not_participating() {
        let p = ChannelParams {
            bypassed: true,
            ..Default::default()
        };
        assert!(!p.is_participating(false));
    }

    #[test]
    fn solo_logic() {
        // When some channel is soloed, non-soloed channels don't participate
        let normal = ChannelParams::default();
        assert!(!normal.is_participating(true));

        let soloed = ChannelParams {
            soloed: true,
            ..Default::default()
        };
        assert!(soloed.is_participating(true));
    }

    #[test]
    fn solo_and_mute() {
        // Muted overrides solo
        let p = ChannelParams {
            soloed: true,
            muted: true,
            ..Default::default()
        };
        assert!(!p.is_participating(true));
    }

    #[test]
    fn channel_creation() {
        let ch = Channel::new(3, 48000.0);
        assert_eq!(ch.index, 3);
        assert_eq!(ch.smoothed_gain, 0.0);
    }
}
