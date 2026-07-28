use crate::math_utils::time_constant_to_coeff;

/// One-pole smoother with asymmetric attack and release coefficients.
///
/// When the input exceeds the current value (rising), the attack coefficient
/// is used. When the input falls below the current value, the release
/// coefficient is used. This gives fast onset and slow decay.
pub struct OnePoleSmoother {
    current: f64,
    attack_coeff: f64,
    release_coeff: f64,
}

impl OnePoleSmoother {
    pub fn new(attack_coeff: f64, release_coeff: f64) -> Self {
        Self {
            current: 0.0,
            attack_coeff,
            release_coeff,
        }
    }

    /// Create from time constants in milliseconds.
    pub fn from_ms(attack_ms: f64, release_ms: f64, sample_rate: f64) -> Self {
        Self {
            current: 0.0,
            attack_coeff: time_constant_to_coeff(attack_ms, sample_rate),
            release_coeff: time_constant_to_coeff(release_ms, sample_rate),
        }
    }

    /// Process one sample and return the smoothed value.
    #[inline]
    pub fn process(&mut self, input: f64) -> f64 {
        let coeff = if input > self.current {
            self.attack_coeff
        } else {
            self.release_coeff
        };
        self.current += coeff * (input - self.current);
        self.current
    }

    /// Set the current value immediately (no smoothing).
    pub fn set_immediate(&mut self, value: f64) {
        self.current = value;
    }

    /// Reset to zero.
    pub fn reset(&mut self) {
        self.current = 0.0;
    }

    #[inline]
    pub fn current(&self) -> f64 {
        self.current
    }

    /// Update coefficients from time constants.
    pub fn set_coefficients(&mut self, attack_ms: f64, release_ms: f64, sample_rate: f64) {
        self.attack_coeff = time_constant_to_coeff(attack_ms, sample_rate);
        self.release_coeff = time_constant_to_coeff(release_ms, sample_rate);
    }
}

/// Linear ramp toward a target, used for bypass crossfades.
///
/// A one-pole smoother approaches its target asymptotically and never lands on
/// it exactly, so a "settled" bypass would still scale the signal by 0.9999….
/// This ramp reaches the target exactly, which is what makes a fully engaged
/// bypass true passthrough.
///
/// `Copy` because the engine snapshots it per channel: the block loop is
/// channel-outer/sample-inner, so each channel walks its own copy and the
/// engine advances the master copy once per block.
#[derive(Debug, Clone, Copy)]
pub struct LinearRamp {
    current: f64,
    target: f64,
    step: f64,
}

impl LinearRamp {
    pub fn new(initial: f64, ramp_ms: f64, sample_rate: f64) -> Self {
        let mut ramp = Self {
            current: initial,
            target: initial,
            step: 1.0,
        };
        ramp.set_time(ramp_ms, sample_rate);
        ramp
    }

    /// Set the time taken to traverse the full 0→1 range.
    pub fn set_time(&mut self, ramp_ms: f64, sample_rate: f64) {
        let samples = ramp_ms * 0.001 * sample_rate;
        self.step = if samples >= 1.0 { 1.0 / samples } else { 1.0 };
    }

    #[inline]
    pub fn set_target(&mut self, target: f64) {
        self.target = target;
    }

    pub fn set_immediate(&mut self, value: f64) {
        self.current = value;
        self.target = value;
    }

    /// Advance one sample and return the new value.
    #[inline]
    pub fn next_value(&mut self) -> f64 {
        self.advance(1);
        self.current
    }

    /// Advance `n` samples. Equivalent to calling `next_value()` n times.
    #[inline]
    pub fn advance(&mut self, n: usize) {
        let delta = self.step * n as f64;
        if self.current < self.target {
            self.current = (self.current + delta).min(self.target);
        } else if self.current > self.target {
            self.current = (self.current - delta).max(self.target);
        }

        // Land on the target instead of creeping toward it. Accumulated
        // rounding over a few hundred steps otherwise leaves the ramp a hair
        // short, and a bypass that settles at 0.99999999 is not passthrough.
        // The snap is at most half a step, far below audibility.
        if (self.target - self.current).abs() < self.step * 0.5 {
            self.current = self.target;
        }
    }

    #[inline]
    pub fn current(&self) -> f64 {
        self.current
    }

    #[inline]
    pub fn is_settled(&self) -> bool {
        self.current == self.target
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn step_response_converges() {
        let mut s = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);
        // Step from 0 to 1: after many iterations, should be near 1.0
        for _ in 0..48000 {
            s.process(1.0);
        }
        assert!((s.current() - 1.0).abs() < 1e-6);
    }

    #[test]
    fn attack_faster_than_release() {
        let mut s_attack = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);
        let mut s_release = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);

        // Attack: rise from 0 to 1
        let rise_samples = 240; // 5ms at 48kHz
        for _ in 0..rise_samples {
            s_attack.process(1.0);
        }
        let after_attack = s_attack.current();

        // Release: first set to 1, then fall to 0
        s_release.set_immediate(1.0);
        for _ in 0..rise_samples {
            s_release.process(0.0);
        }
        let after_release = s_release.current();

        // Attack should have risen more than release has fallen
        assert!(after_attack > (1.0 - after_release));
    }

    #[test]
    fn reset_to_zero() {
        let mut s = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);
        for _ in 0..1000 {
            s.process(1.0);
        }
        s.reset();
        assert_eq!(s.current(), 0.0);
    }

    #[test]
    fn set_immediate() {
        let mut s = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);
        s.set_immediate(0.75);
        assert_eq!(s.current(), 0.75);
    }

    #[test]
    fn constant_input_converges() {
        let mut s = OnePoleSmoother::from_ms(5.0, 150.0, 48000.0);
        let target = 0.5;
        for _ in 0..48000 {
            s.process(target);
        }
        assert!((s.current() - target).abs() < 1e-6);
    }

    #[test]
    fn ramp_reaches_target_exactly() {
        // The whole reason this is a linear ramp and not a one-pole: a settled
        // bypass must be exactly 1.0, not 0.9999.
        let mut r = LinearRamp::new(0.0, 15.0, 48000.0);
        r.set_target(1.0);
        for _ in 0..720 {
            r.next_value();
        }
        assert_eq!(r.current(), 1.0);
        assert!(r.is_settled());
    }

    #[test]
    fn ramp_does_not_overshoot() {
        let mut r = LinearRamp::new(0.0, 15.0, 48000.0);
        r.set_target(1.0);
        for _ in 0..10_000 {
            let v = r.next_value();
            assert!((0.0..=1.0).contains(&v), "ramp left [0,1]: {v}");
        }
        assert_eq!(r.current(), 1.0);
    }

    #[test]
    fn ramp_advance_matches_repeated_next() {
        let mut stepwise = LinearRamp::new(0.0, 15.0, 48000.0);
        let mut bulk = LinearRamp::new(0.0, 15.0, 48000.0);
        stepwise.set_target(1.0);
        bulk.set_target(1.0);

        for _ in 0..256 {
            stepwise.next_value();
        }
        bulk.advance(256);

        assert!(
            (stepwise.current() - bulk.current()).abs() < 1e-12,
            "advance({}) diverged from repeated next_value(): {} vs {}",
            256,
            bulk.current(),
            stepwise.current()
        );
    }

    #[test]
    fn ramp_reverses_mid_flight() {
        let mut r = LinearRamp::new(0.0, 15.0, 48000.0);
        r.set_target(1.0);
        r.advance(360);
        let midpoint = r.current();
        assert!(
            midpoint > 0.4 && midpoint < 0.6,
            "expected ~0.5, got {midpoint}"
        );

        r.set_target(0.0);
        r.advance(360);
        assert_eq!(r.current(), 0.0);
    }

    #[test]
    fn ramp_time_shorter_than_one_sample_is_instant() {
        let mut r = LinearRamp::new(0.0, 0.0, 48000.0);
        r.set_target(1.0);
        assert_eq!(r.next_value(), 1.0);
    }
}
