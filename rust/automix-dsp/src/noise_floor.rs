use crate::constants::{
    DEFAULT_NOISE_FLOOR_MARGIN_DB, NOISE_FLOOR_FALL_MS, NOISE_FLOOR_INIT_DB,
    NOISE_FLOOR_RISE_ACTIVE_MS, NOISE_FLOOR_RISE_MS,
};
use crate::math_utils::{db_to_linear, linear_to_db, time_constant_to_coeff};

/// Adaptive noise floor tracker.
///
/// Follows the room's resting level so activity detection adapts to the venue
/// instead of gating at a fixed threshold. Three rates, chosen by where the
/// input sits relative to the current floor:
///
/// - Below the floor: fall fast, so the tracker finds a quiet room quickly.
/// - Above the floor but below the activity margin: rise at a moderate rate.
/// - Above the activity margin (someone is talking): rise very slowly, so
///   speech does not drag the floor up behind it.
///
/// The third rate is non-zero on purpose. An earlier version held the floor
/// completely still whenever the input was above the margin, which meant a room
/// whose ambient level rose past the margin — an HVAC system starting, a crowd
/// filling in — left every channel reading "active" permanently with no way to
/// recover.
///
/// Coefficients are per-sample but `update` is called once per block, so the
/// per-sample coefficient is raised to the block length. Without that the wall
/// clock time constants scale with the host's buffer size, which made the
/// nominal 500 ms fall take anywhere from 30 seconds to 20 minutes depending on
/// the host.
///
/// The state is kept in dB, not linear amplitude. Smoothing a linear level
/// toward a target 60 dB away moves it most of the way in dB terms after a few
/// percent of linear travel, so a "slow" linear tracker still lurches tens of
/// dB in a couple of seconds. In the log domain the rate means what it says.
pub struct NoiseFloorTracker {
    floor_db: f64,
    fall_coeff: f64,
    rise_coeff: f64,
    rise_active_coeff: f64,
    margin_db: f64,
}

/// Compound a per-sample one-pole coefficient over `n` samples.
///
/// Applying `y += a(x - y)` n times is exactly `y += (1 - (1-a)^n)(x - y)`, so
/// this stays a closed form rather than a loop.
#[inline]
fn block_coeff(per_sample: f64, n: usize) -> f64 {
    if n == 0 {
        return 0.0;
    }
    let remaining = (1.0 - per_sample).max(0.0);
    1.0 - remaining.powi(n.min(i32::MAX as usize) as i32)
}

impl NoiseFloorTracker {
    pub fn new(sample_rate: f64) -> Self {
        Self {
            floor_db: NOISE_FLOOR_INIT_DB,
            fall_coeff: time_constant_to_coeff(NOISE_FLOOR_FALL_MS, sample_rate),
            rise_coeff: time_constant_to_coeff(NOISE_FLOOR_RISE_MS, sample_rate),
            rise_active_coeff: time_constant_to_coeff(NOISE_FLOOR_RISE_ACTIVE_MS, sample_rate),
            margin_db: DEFAULT_NOISE_FLOOR_MARGIN_DB,
        }
    }

    /// Update the floor estimate from this block's RMS level.
    ///
    /// `num_samples` is the block length the RMS was measured over; it keeps
    /// the time constants tied to wall-clock time rather than buffer size.
    pub fn update(&mut self, rms_linear: f64, num_samples: usize) {
        if !rms_linear.is_finite() {
            return;
        }

        let rms_db = linear_to_db(rms_linear);

        let per_sample = if rms_db < self.floor_db {
            self.fall_coeff
        } else if rms_db > self.floor_db + self.margin_db {
            self.rise_active_coeff
        } else {
            self.rise_coeff
        };

        let coeff = block_coeff(per_sample, num_samples);
        self.floor_db += coeff * (rms_db - self.floor_db);
    }

    /// Whether the given RMS level is above the floor plus its margin.
    #[inline]
    pub fn is_active(&self, rms_linear: f64) -> bool {
        linear_to_db(rms_linear) > self.floor_db + self.margin_db
    }

    /// Current noise floor level in linear units.
    #[inline]
    pub fn floor_linear(&self) -> f64 {
        db_to_linear(self.floor_db)
    }

    /// Current noise floor level in dB.
    #[inline]
    pub fn floor_db(&self) -> f64 {
        self.floor_db
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    const SR: f64 = 48000.0;
    const BLOCK: usize = 256;

    /// Drive the tracker for `seconds` of wall-clock audio at a fixed level,
    /// at block rate — which is how the engine actually calls it.
    fn run_for(nf: &mut NoiseFloorTracker, level: f64, seconds: f64, block: usize) {
        let blocks = (seconds * SR / block as f64).round() as usize;
        for _ in 0..blocks {
            nf.update(level, block);
        }
    }

    #[test]
    fn tracks_downward() {
        let mut nf = NoiseFloorTracker::new(SR);
        let quiet = db_to_linear(-80.0);
        run_for(&mut nf, quiet, 5.0, BLOCK);
        assert!(
            nf.floor_db() < -70.0,
            "floor should have fallen toward -80 dB, got {}",
            nf.floor_db()
        );
    }

    /// The bug this guards: the coefficients are per-sample but update() runs
    /// once per block, so without compounding the convergence rate scaled with
    /// the host's buffer size. Same wall-clock time must give the same floor.
    #[test]
    fn convergence_is_independent_of_block_size() {
        let quiet = db_to_linear(-80.0);
        let mut floors = Vec::new();

        for block in [64_usize, 256, 1024, 2048] {
            let mut nf = NoiseFloorTracker::new(SR);
            run_for(&mut nf, quiet, 5.0, block);
            floors.push(nf.floor_db());
        }

        let min = floors.iter().cloned().fold(f64::INFINITY, f64::min);
        let max = floors.iter().cloned().fold(f64::NEG_INFINITY, f64::max);
        assert!(
            max - min < 1.0,
            "floor after 5s varied with block size: {floors:?}"
        );
    }

    #[test]
    fn resists_upward_from_speech() {
        let mut nf = NoiseFloorTracker::new(SR);
        run_for(&mut nf, db_to_linear(-80.0), 5.0, BLOCK);
        let before = nf.floor_db();

        run_for(&mut nf, db_to_linear(-20.0), 2.0, BLOCK);
        let after = nf.floor_db();

        assert!(
            (after - before).abs() < 3.0,
            "speech pulled the floor: before={before}, after={after}"
        );
    }

    /// The lockout regression: if the room gets genuinely louder, the floor has
    /// to follow eventually or every channel reads active forever.
    #[test]
    fn recovers_when_ambient_level_rises() {
        let mut nf = NoiseFloorTracker::new(SR);
        run_for(&mut nf, db_to_linear(-80.0), 5.0, BLOCK);
        assert!(nf.floor_db() < -70.0);

        // Room noise jumps well above the old floor plus margin and stays there.
        let louder = db_to_linear(-45.0);
        assert!(
            nf.is_active(louder),
            "precondition: reads as active at first"
        );

        run_for(&mut nf, louder, 240.0, BLOCK);

        assert!(
            !nf.is_active(louder),
            "floor latched at {} dB and never learned the new ambient level",
            nf.floor_db()
        );
    }

    #[test]
    fn active_detection_with_margin() {
        let mut nf = NoiseFloorTracker::new(SR);
        let quiet = db_to_linear(-80.0);
        run_for(&mut nf, quiet, 5.0, BLOCK);

        assert!(nf.is_active(db_to_linear(-40.0)));
        assert!(!nf.is_active(quiet));
    }

    #[test]
    fn non_finite_input_is_ignored() {
        let mut nf = NoiseFloorTracker::new(SR);
        run_for(&mut nf, db_to_linear(-80.0), 5.0, BLOCK);
        let before = nf.floor_db();

        nf.update(f64::NAN, BLOCK);
        nf.update(f64::INFINITY, BLOCK);

        assert_relative_eq!(nf.floor_db(), before, epsilon = 1e-12);
        assert!(nf.floor_linear().is_finite());
    }

    #[test]
    fn zero_length_block_is_a_no_op() {
        let mut nf = NoiseFloorTracker::new(SR);
        let before = nf.floor_db();
        nf.update(db_to_linear(-10.0), 0);
        assert_relative_eq!(nf.floor_db(), before, epsilon = 1e-12);
    }
}
