use crate::constants::{
    DEFAULT_NOISE_FLOOR_FALL_MS, DEFAULT_NOISE_FLOOR_MARGIN_DB, DEFAULT_NOISE_FLOOR_RISE_MS,
    NOISE_FLOOR_INIT_DB,
};
use crate::math_utils::{db_to_linear, linear_to_db};
use crate::smoothing::OnePoleSmoother;

/// Adaptive noise floor tracker.
///
/// Follows the minimum signal level using a min-follower approach:
/// - Tracks downward quickly (rise coefficient) toward the noise floor.
/// - Resists upward pull when the signal is clearly active (above floor + margin).
/// - On re-enable (after reset), starts at a high floor and tracks down.
pub struct NoiseFloorTracker {
    floor_level: f64,
    smoother: OnePoleSmoother,
    margin_linear: f64,
    initialized: bool,
}

impl NoiseFloorTracker {
    pub fn new(sample_rate: f64) -> Self {
        let init_linear = db_to_linear(NOISE_FLOOR_INIT_DB);
        // For the noise floor, "falling" (tracking downward) should be fast,
        // "rising" (tracking upward) should be slow. The OnePoleSmoother uses
        // attack for input > current, release for input < current.
        // So: attack = slow rise (FALL_MS), release = fast fall (RISE_MS).
        let mut smoother = OnePoleSmoother::from_ms(
            DEFAULT_NOISE_FLOOR_FALL_MS,
            DEFAULT_NOISE_FLOOR_RISE_MS,
            sample_rate,
        );
        smoother.set_immediate(init_linear);

        Self {
            floor_level: init_linear,
            smoother,
            margin_linear: db_to_linear(DEFAULT_NOISE_FLOOR_MARGIN_DB),
            initialized: false,
        }
    }

    /// Update the noise floor estimate given the current RMS level (linear).
    ///
    /// The tracker only follows the signal downward. When the input is
    /// significantly above the current floor (active speech), the floor
    /// is not pulled upward.
    pub fn update(&mut self, rms_linear: f64) {
        if !self.initialized {
            self.initialized = true;
        }

        // Only track toward the input if it's below or near the current floor.
        // If the signal is well above the floor (active speech), don't follow it up.
        if rms_linear < self.floor_level * self.margin_linear {
            self.floor_level = self.smoother.process(rms_linear);
        } else {
            // Still run the smoother but toward the current floor (maintain state)
            self.floor_level = self.smoother.process(self.floor_level);
        }
    }

    /// Check if the given RMS level is above the noise floor + margin (i.e., active).
    #[inline]
    pub fn is_active(&self, rms_linear: f64) -> bool {
        rms_linear > self.floor_level * self.margin_linear
    }

    /// Current noise floor level in linear units.
    #[inline]
    pub fn floor_linear(&self) -> f64 {
        self.floor_level
    }

    /// Current noise floor level in dB.
    #[inline]
    pub fn floor_db(&self) -> f64 {
        linear_to_db(self.floor_level)
    }

    /// Reset the noise floor to initial state (gradual reset).
    pub fn reset(&mut self, sample_rate: f64) {
        let init_linear = db_to_linear(NOISE_FLOOR_INIT_DB);
        self.floor_level = init_linear;
        self.smoother = OnePoleSmoother::from_ms(
            DEFAULT_NOISE_FLOOR_FALL_MS,
            DEFAULT_NOISE_FLOOR_RISE_MS,
            sample_rate,
        );
        self.smoother.set_immediate(init_linear);
        self.initialized = false;
    }

    /// Set the margin in dB.
    pub fn set_margin_db(&mut self, margin_db: f64) {
        self.margin_linear = db_to_linear(margin_db);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    #[test]
    fn tracks_downward() {
        let mut nf = NoiseFloorTracker::new(48000.0);
        let quiet = db_to_linear(-80.0);
        // Feed quiet signal many times
        for _ in 0..48000 {
            nf.update(quiet);
        }
        // Floor should have tracked down toward -80dB
        assert!(nf.floor_db() < -70.0);
    }

    #[test]
    fn resists_upward_from_speech() {
        let mut nf = NoiseFloorTracker::new(48000.0);
        let quiet = db_to_linear(-80.0);
        // First, let it track to a low floor
        for _ in 0..48000 {
            nf.update(quiet);
        }
        let floor_before = nf.floor_db();

        // Now feed a loud signal (speech)
        let loud = db_to_linear(-20.0);
        for _ in 0..4800 {
            nf.update(loud);
        }
        let floor_after = nf.floor_db();

        // Floor should not have risen significantly
        assert!(
            (floor_after - floor_before).abs() < 3.0,
            "Floor moved too much: before={floor_before}, after={floor_after}"
        );
    }

    #[test]
    fn active_detection_with_margin() {
        let mut nf = NoiseFloorTracker::new(48000.0);
        let quiet = db_to_linear(-80.0);
        for _ in 0..48000 {
            nf.update(quiet);
        }

        // Signal well above floor + margin should be active
        let loud = db_to_linear(-40.0);
        assert!(nf.is_active(loud));

        // Signal at floor level should not be active
        assert!(!nf.is_active(quiet));
    }

    #[test]
    fn gradual_reset() {
        let mut nf = NoiseFloorTracker::new(48000.0);
        let quiet = db_to_linear(-80.0);
        for _ in 0..48000 {
            nf.update(quiet);
        }
        assert!(nf.floor_db() < -70.0);

        // Reset — floor should jump back to init level
        nf.reset(48000.0);
        assert_relative_eq!(nf.floor_db(), NOISE_FLOOR_INIT_DB, epsilon = 1.0);
    }
}
