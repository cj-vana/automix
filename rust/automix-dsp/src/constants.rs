/// Maximum number of channels the engine supports.
pub const AUTOMIX_MAX_CHANNELS: usize = 32;

/// Maximum block size in samples.
pub const AUTOMIX_MAX_BLOCK_SIZE: usize = 4096;

// --- Internal constants (not exported to C header) ---

/// Default RMS detection window in milliseconds.
pub(crate) const DEFAULT_RMS_WINDOW_MS: f64 = 20.0;

/// How fast the noise floor tracks *down* toward a quieter room, in ms.
pub(crate) const NOISE_FLOOR_FALL_MS: f64 = 500.0;

/// How fast the noise floor tracks *up* toward a louder room while the channel
/// is idle, in ms.
pub(crate) const NOISE_FLOOR_RISE_MS: f64 = 5000.0;

/// How fast the noise floor tracks up while the channel is carrying signal.
///
/// Deliberately far slower than the idle rise: speech must not drag the floor
/// up behind it, but the tracker still has to rise eventually or a room whose
/// ambient level genuinely increases latches the channel open forever.
pub(crate) const NOISE_FLOOR_RISE_ACTIVE_MS: f64 = 60_000.0;

/// Default noise floor margin above tracked floor, in dB.
pub(crate) const DEFAULT_NOISE_FLOOR_MARGIN_DB: f64 = 6.0;

/// Initial noise floor level in dB (high, so it tracks down quickly).
pub(crate) const NOISE_FLOOR_INIT_DB: f64 = -60.0;

/// Default gain smoothing attack time in milliseconds.
pub(crate) const DEFAULT_ATTACK_MS: f64 = 5.0;

/// Default gain smoothing release time in milliseconds.
pub(crate) const DEFAULT_RELEASE_MS: f64 = 150.0;

/// Default last-mic-hold time in milliseconds.
pub(crate) const DEFAULT_HOLD_TIME_MS: f64 = 500.0;

/// Crossfade time for global bypass, in milliseconds.
///
/// Short enough to feel instant to an operator hitting bypass mid-show, long
/// enough that the gain discontinuity is inaudible rather than a click.
pub(crate) const BYPASS_RAMP_MS: f64 = 15.0;

/// Small value to prevent division by zero.
pub(crate) const EPSILON: f64 = 1e-10;

/// Threshold below which a signal is considered silence, in dB.
pub(crate) const SILENCE_THRESHOLD_DB: f64 = -120.0;

/// Default channel weight (linear, 0.0–1.0).
pub(crate) const DEFAULT_WEIGHT: f64 = 1.0;

// --- Min/max bounds for configurable parameters ---

pub(crate) const MIN_WEIGHT: f64 = 0.0;
pub(crate) const MAX_WEIGHT: f64 = 1.0;

pub(crate) const MIN_ATTACK_MS: f64 = 0.1;
pub(crate) const MAX_ATTACK_MS: f64 = 100.0;

pub(crate) const MIN_RELEASE_MS: f64 = 1.0;
pub(crate) const MAX_RELEASE_MS: f64 = 1000.0;

pub(crate) const MIN_HOLD_TIME_MS: f64 = 0.0;
pub(crate) const MAX_HOLD_TIME_MS: f64 = 5000.0;

#[allow(dead_code)]
pub(crate) const MIN_RMS_WINDOW_MS: f64 = 1.0;
#[allow(dead_code)]
pub(crate) const MAX_RMS_WINDOW_MS: f64 = 100.0;

#[allow(dead_code)]
pub(crate) const MIN_NOISE_FLOOR_MARGIN_DB: f64 = 0.0;
#[allow(dead_code)]
pub(crate) const MAX_NOISE_FLOOR_MARGIN_DB: f64 = 24.0;
