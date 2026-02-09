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
}
