use crate::constants::SILENCE_THRESHOLD_DB;

/// Convert a linear amplitude to decibels.
/// Values at or below zero are clamped to `SILENCE_THRESHOLD_DB`.
#[inline]
pub fn linear_to_db(linear: f64) -> f64 {
    if linear <= 0.0 {
        SILENCE_THRESHOLD_DB
    } else {
        let db = 20.0 * linear.log10();
        if db < SILENCE_THRESHOLD_DB {
            SILENCE_THRESHOLD_DB
        } else {
            db
        }
    }
}

/// Convert decibels to a linear amplitude.
#[inline]
pub fn db_to_linear(db: f64) -> f64 {
    10.0_f64.powf(db / 20.0)
}

/// Compute a one-pole filter coefficient from a time constant in milliseconds
/// and sample rate. Returns the alpha value for: `y = alpha * x + (1 - alpha) * y_prev`.
#[inline]
pub fn time_constant_to_coeff(time_ms: f64, sample_rate: f64) -> f64 {
    if time_ms <= 0.0 || sample_rate <= 0.0 {
        return 1.0; // instant response
    }
    let samples = time_ms * 0.001 * sample_rate;
    1.0 - (-1.0 / samples).exp()
}

/// Convert milliseconds to a sample count.
#[inline]
pub fn ms_to_samples(ms: f64, sample_rate: f64) -> usize {
    (ms * 0.001 * sample_rate).round() as usize
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    #[test]
    fn test_linear_to_db_known_values() {
        assert_relative_eq!(linear_to_db(1.0), 0.0, epsilon = 1e-10);
        assert_relative_eq!(linear_to_db(0.1), -20.0, epsilon = 1e-10);
        assert_relative_eq!(linear_to_db(0.01), -40.0, epsilon = 1e-10);
    }

    #[test]
    fn test_linear_to_db_zero() {
        assert_eq!(linear_to_db(0.0), SILENCE_THRESHOLD_DB);
    }

    #[test]
    fn test_linear_to_db_negative() {
        assert_eq!(linear_to_db(-1.0), SILENCE_THRESHOLD_DB);
    }

    #[test]
    fn test_db_to_linear_known_values() {
        assert_relative_eq!(db_to_linear(0.0), 1.0, epsilon = 1e-10);
        assert_relative_eq!(db_to_linear(-20.0), 0.1, epsilon = 1e-10);
        assert_relative_eq!(db_to_linear(-40.0), 0.01, epsilon = 1e-10);
    }

    #[test]
    fn test_roundtrip() {
        for &val in &[0.001, 0.01, 0.1, 0.5, 1.0] {
            let db = linear_to_db(val);
            let back = db_to_linear(db);
            assert_relative_eq!(back, val, epsilon = 1e-9);
        }
    }

    #[test]
    fn test_time_constant_to_coeff() {
        // With very large time constant, alpha should be small
        let alpha_slow = time_constant_to_coeff(1000.0, 48000.0);
        let alpha_fast = time_constant_to_coeff(1.0, 48000.0);
        assert!(alpha_fast > alpha_slow);
        assert!(alpha_slow > 0.0 && alpha_slow < 1.0);
        assert!(alpha_fast > 0.0 && alpha_fast < 1.0);
    }

    #[test]
    fn test_time_constant_zero_is_instant() {
        assert_eq!(time_constant_to_coeff(0.0, 48000.0), 1.0);
    }

    #[test]
    fn test_ms_to_samples() {
        assert_eq!(ms_to_samples(1000.0, 48000.0), 48000);
        assert_eq!(ms_to_samples(20.0, 48000.0), 960);
        assert_eq!(ms_to_samples(0.0, 48000.0), 0);
    }
}
