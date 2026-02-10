use crate::constants::{AUTOMIX_MAX_CHANNELS, EPSILON};

/// Result of a Dugan gain-sharing computation.
#[derive(Debug, Clone)]
pub struct GainSharingResult {
    /// Per-channel gains in [0, 1].
    pub gains: [f64; AUTOMIX_MAX_CHANNELS],
    /// Number of open microphones.
    pub nom: f64,
}

/// Compute Dugan gain-sharing gains from current RMS levels.
///
/// This is a pure, stateless function. Given channel RMS levels, weights,
/// activity flags, and participation flags, it produces per-channel gains
/// that sum to approximately 1.0 across participating active channels.
///
/// - `rms_levels`: Current RMS level per channel (linear).
/// - `weights`: Linear weight multiplier per channel (0.0–1.0).
/// - `is_active`: Whether each channel is above the noise floor.
/// - `participating`: Whether each channel participates in gain-sharing
///   (i.e., not muted, not bypassed, and passes solo logic).
/// - `num_channels`: Number of channels in use.
/// - `last_mic_channel`: Channel to hold at unity when all channels are silent.
pub fn compute_dugan_gains(
    rms_levels: &[f64; AUTOMIX_MAX_CHANNELS],
    weights: &[f64; AUTOMIX_MAX_CHANNELS],
    is_active: &[bool; AUTOMIX_MAX_CHANNELS],
    participating: &[bool; AUTOMIX_MAX_CHANNELS],
    num_channels: usize,
    last_mic_channel: Option<usize>,
) -> GainSharingResult {
    let mut gains = [0.0_f64; AUTOMIX_MAX_CHANNELS];
    let mut weighted_sum = 0.0_f64;
    let mut nom = 0.0_f64;

    // Compute weighted levels and sum for participating+active channels
    let mut weighted = [0.0_f64; AUTOMIX_MAX_CHANNELS];
    for i in 0..num_channels {
        if participating[i] && is_active[i] {
            weighted[i] = rms_levels[i] * weights[i];
            weighted_sum += weighted[i];
            nom += 1.0;
        }
    }

    if weighted_sum > EPSILON {
        // Normal Dugan: distribute gain proportional to weighted RMS
        for i in 0..num_channels {
            if participating[i] && is_active[i] {
                gains[i] = weighted[i] / weighted_sum;
            }
            // Non-participating or inactive channels remain at 0.0
        }
    } else {
        // Silence fallback: if a last-mic-hold channel exists, give it unity
        if let Some(ch) = last_mic_channel {
            if ch < num_channels && participating[ch] {
                gains[ch] = 1.0;
                nom = 1.0;
            }
        }
    }

    GainSharingResult { gains, nom }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    fn make_arrays(
        rms: &[f64],
        weights: &[f64],
        active: &[bool],
        participating: &[bool],
    ) -> (
        [f64; AUTOMIX_MAX_CHANNELS],
        [f64; AUTOMIX_MAX_CHANNELS],
        [bool; AUTOMIX_MAX_CHANNELS],
        [bool; AUTOMIX_MAX_CHANNELS],
    ) {
        let mut r = [0.0; AUTOMIX_MAX_CHANNELS];
        let mut w = [1.0; AUTOMIX_MAX_CHANNELS];
        let mut a = [false; AUTOMIX_MAX_CHANNELS];
        let mut p = [false; AUTOMIX_MAX_CHANNELS];
        for (i, &v) in rms.iter().enumerate() {
            r[i] = v;
        }
        for (i, &v) in weights.iter().enumerate() {
            w[i] = v;
        }
        for (i, &v) in active.iter().enumerate() {
            a[i] = v;
        }
        for (i, &v) in participating.iter().enumerate() {
            p[i] = v;
        }
        (r, w, a, p)
    }

    #[test]
    fn single_active_channel() {
        let (rms, w, a, p) = make_arrays(&[0.5], &[1.0], &[true], &[true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 1, None);
        assert_relative_eq!(result.gains[0], 1.0, epsilon = 1e-10);
        assert_relative_eq!(result.nom, 1.0, epsilon = 1e-10);
    }

    #[test]
    fn two_equal_channels() {
        let (rms, w, a, p) = make_arrays(&[0.5, 0.5], &[1.0, 1.0], &[true, true], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        assert_relative_eq!(result.gains[0], 0.5, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.5, epsilon = 1e-10);
        assert_relative_eq!(result.nom, 2.0, epsilon = 1e-10);
    }

    #[test]
    fn proportional_distribution() {
        // Channel 0 is 3x louder than channel 1
        let (rms, w, a, p) = make_arrays(&[0.75, 0.25], &[1.0, 1.0], &[true, true], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        assert_relative_eq!(result.gains[0], 0.75, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.25, epsilon = 1e-10);
    }

    #[test]
    fn inactive_channel_gets_zero() {
        let (rms, w, a, p) = make_arrays(&[0.5, 0.5], &[1.0, 1.0], &[true, false], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        assert_relative_eq!(result.gains[0], 1.0, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.0, epsilon = 1e-10);
        assert_relative_eq!(result.nom, 1.0, epsilon = 1e-10);
    }

    #[test]
    fn non_participating_excluded() {
        let (rms, w, a, p) = make_arrays(&[0.5, 0.5], &[1.0, 1.0], &[true, true], &[true, false]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        assert_relative_eq!(result.gains[0], 1.0, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.0, epsilon = 1e-10);
    }

    #[test]
    fn silence_with_last_mic_hold() {
        let (rms, w, a, p) = make_arrays(&[0.0, 0.0], &[1.0, 1.0], &[false, false], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, Some(1));
        assert_relative_eq!(result.gains[0], 0.0, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 1.0, epsilon = 1e-10);
    }

    #[test]
    fn silence_no_last_mic() {
        let (rms, w, a, p) = make_arrays(&[0.0, 0.0], &[1.0, 1.0], &[false, false], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        assert_relative_eq!(result.gains[0], 0.0, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.0, epsilon = 1e-10);
    }

    #[test]
    fn weights_affect_distribution() {
        // Equal RMS but channel 0 has double weight
        let (rms, w, a, p) = make_arrays(&[0.5, 0.5], &[1.0, 0.5], &[true, true], &[true, true]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, None);
        // weighted: 0.5*1.0=0.5, 0.5*0.5=0.25, sum=0.75
        assert_relative_eq!(result.gains[0], 0.5 / 0.75, epsilon = 1e-10);
        assert_relative_eq!(result.gains[1], 0.25 / 0.75, epsilon = 1e-10);
    }

    #[test]
    fn gains_sum_to_one() {
        let (rms, w, a, p) = make_arrays(
            &[0.1, 0.3, 0.2, 0.4],
            &[1.0, 0.8, 1.0, 0.5],
            &[true, true, true, true],
            &[true, true, true, true],
        );
        let result = compute_dugan_gains(&rms, &w, &a, &p, 4, None);
        let sum: f64 = result.gains.iter().sum();
        assert_relative_eq!(sum, 1.0, epsilon = 1e-10);
    }

    #[test]
    fn last_mic_hold_non_participating_ignored() {
        // Last mic channel is non-participating — should not get gain
        let (rms, w, a, p) = make_arrays(&[0.0, 0.0], &[1.0, 1.0], &[false, false], &[true, false]);
        let result = compute_dugan_gains(&rms, &w, &a, &p, 2, Some(1));
        assert_relative_eq!(result.gains[1], 0.0, epsilon = 1e-10);
    }
}

#[cfg(test)]
mod proptests {
    use super::*;
    use proptest::prelude::*;

    fn arb_channel_count() -> impl Strategy<Value = usize> {
        1..=32_usize
    }

    fn arb_rms(n: usize) -> impl Strategy<Value = Vec<f64>> {
        proptest::collection::vec(0.0001..1.0_f64, n)
    }

    fn arb_weights(n: usize) -> impl Strategy<Value = Vec<f64>> {
        proptest::collection::vec(0.1..1.0_f64, n)
    }

    proptest! {
        #[test]
        fn gains_sum_approximately_one(
            n in arb_channel_count(),
            rms_vec in arb_channel_count().prop_flat_map(arb_rms),
            weights_vec in arb_channel_count().prop_flat_map(arb_weights),
        ) {
            let n = n.min(rms_vec.len()).min(weights_vec.len());
            let mut rms = [0.0; AUTOMIX_MAX_CHANNELS];
            let mut weights = [1.0; AUTOMIX_MAX_CHANNELS];
            let mut active = [false; AUTOMIX_MAX_CHANNELS];
            let mut participating = [false; AUTOMIX_MAX_CHANNELS];
            for i in 0..n {
                rms[i] = rms_vec[i];
                weights[i] = weights_vec[i];
                active[i] = true;
                participating[i] = true;
            }
            let result = compute_dugan_gains(&rms, &weights, &active, &participating, n, None);
            let sum: f64 = result.gains[..n].iter().sum();
            prop_assert!((sum - 1.0).abs() < 1e-8,
                "Gain sum should be ~1.0, got {sum}");
        }

        #[test]
        fn all_gains_bounded(
            n in arb_channel_count(),
            rms_vec in arb_channel_count().prop_flat_map(arb_rms),
        ) {
            let n = n.min(rms_vec.len());
            let mut rms = [0.0; AUTOMIX_MAX_CHANNELS];
            let mut active = [false; AUTOMIX_MAX_CHANNELS];
            let mut participating = [false; AUTOMIX_MAX_CHANNELS];
            let weights = [1.0; AUTOMIX_MAX_CHANNELS];
            for i in 0..n {
                rms[i] = rms_vec[i];
                active[i] = true;
                participating[i] = true;
            }
            let result = compute_dugan_gains(&rms, &weights, &active, &participating, n, None);
            for i in 0..AUTOMIX_MAX_CHANNELS {
                prop_assert!(result.gains[i] >= 0.0 && result.gains[i] <= 1.0,
                    "Gain[{i}] = {} out of bounds", result.gains[i]);
            }
        }

        #[test]
        fn louder_gets_more_gain(
            quiet in 0.001..0.3_f64,
            loud_extra in 0.01..0.7_f64,
        ) {
            let loud = quiet + loud_extra;
            let mut rms = [0.0; AUTOMIX_MAX_CHANNELS];
            let weights = [1.0; AUTOMIX_MAX_CHANNELS];
            let mut active = [false; AUTOMIX_MAX_CHANNELS];
            let mut participating = [false; AUTOMIX_MAX_CHANNELS];
            rms[0] = loud;
            rms[1] = quiet;
            active[0] = true;
            active[1] = true;
            participating[0] = true;
            participating[1] = true;
            let result = compute_dugan_gains(&rms, &weights, &active, &participating, 2, None);
            prop_assert!(result.gains[0] > result.gains[1],
                "Louder channel gain {} should exceed quieter {}",
                result.gains[0], result.gains[1]);
        }

        #[test]
        fn deterministic(
            n in arb_channel_count(),
            rms_vec in arb_channel_count().prop_flat_map(arb_rms),
        ) {
            let n = n.min(rms_vec.len());
            let mut rms = [0.0; AUTOMIX_MAX_CHANNELS];
            let weights = [1.0; AUTOMIX_MAX_CHANNELS];
            let mut active = [false; AUTOMIX_MAX_CHANNELS];
            let mut participating = [false; AUTOMIX_MAX_CHANNELS];
            for i in 0..n {
                rms[i] = rms_vec[i];
                active[i] = true;
                participating[i] = true;
            }
            let r1 = compute_dugan_gains(&rms, &weights, &active, &participating, n, None);
            let r2 = compute_dugan_gains(&rms, &weights, &active, &participating, n, None);
            for i in 0..AUTOMIX_MAX_CHANNELS {
                prop_assert!((r1.gains[i] - r2.gains[i]).abs() < 1e-15,
                    "Non-deterministic at channel {i}");
            }
        }
    }
}
