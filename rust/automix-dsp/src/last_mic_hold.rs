use crate::constants::{AUTOMIX_MAX_CHANNELS, DEFAULT_HOLD_TIME_MS};
use crate::math_utils::ms_to_samples;

/// Tracks the last active microphone channel and holds it open for a
/// configurable duration after all channels go silent.
pub struct LastMicHold {
    last_active_channel: Option<usize>,
    hold_counter: u64,
    hold_duration: u64,
    is_holding: bool,
}

impl LastMicHold {
    pub fn new(sample_rate: f64) -> Self {
        Self {
            last_active_channel: None,
            hold_counter: 0,
            hold_duration: ms_to_samples(DEFAULT_HOLD_TIME_MS, sample_rate) as u64,
            is_holding: false,
        }
    }

    /// Update the hold state given current activity and participation flags.
    ///
    /// Returns the channel index to hold at unity gain (if any).
    pub fn update(
        &mut self,
        is_active: &[bool; AUTOMIX_MAX_CHANNELS],
        participating: &[bool; AUTOMIX_MAX_CHANNELS],
        num_channels: usize,
        block_size: usize,
    ) -> Option<usize> {
        // Check if any channel is currently active and participating
        let mut any_active = false;
        let mut last_found = None;
        for i in 0..num_channels {
            if participating[i] && is_active[i] {
                any_active = true;
                last_found = Some(i);
            }
        }

        if any_active {
            // Update last active channel and reset hold counter
            if let Some(ch) = last_found {
                self.last_active_channel = Some(ch);
            }
            self.hold_counter = 0;
            self.is_holding = false;
            None // No hold needed — there are active channels
        } else if self.hold_duration == 0 {
            // Hold disabled
            self.is_holding = false;
            None
        } else if let Some(ch) = self.last_active_channel {
            // No active channels — check if held channel is still participating
            if !participating[ch] {
                // Held channel was muted/removed — release hold
                self.is_holding = false;
                self.last_active_channel = None;
                return None;
            }

            // Advance hold counter
            self.hold_counter += block_size as u64;
            if self.hold_counter < self.hold_duration {
                self.is_holding = true;
                Some(ch)
            } else {
                // Hold expired
                self.is_holding = false;
                None
            }
        } else {
            // No last active channel known
            self.is_holding = false;
            None
        }
    }

    /// Set the hold duration in milliseconds.
    pub fn set_hold_time_ms(&mut self, ms: f64, sample_rate: f64) {
        self.hold_duration = ms_to_samples(ms, sample_rate) as u64;
    }

    /// Reset the hold state.
    pub fn reset(&mut self) {
        self.last_active_channel = None;
        self.hold_counter = 0;
        self.is_holding = false;
    }

    #[inline]
    pub fn is_holding(&self) -> bool {
        self.is_holding
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::constants::AUTOMIX_MAX_CHANNELS;

    fn make_flags(
        active: &[bool],
        part: &[bool],
    ) -> ([bool; AUTOMIX_MAX_CHANNELS], [bool; AUTOMIX_MAX_CHANNELS]) {
        let mut a = [false; AUTOMIX_MAX_CHANNELS];
        let mut p = [false; AUTOMIX_MAX_CHANNELS];
        for (i, &v) in active.iter().enumerate() {
            a[i] = v;
        }
        for (i, &v) in part.iter().enumerate() {
            p[i] = v;
        }
        (a, p)
    }

    #[test]
    fn no_hold_when_active() {
        let mut h = LastMicHold::new(48000.0);
        let (a, p) = make_flags(&[true, false], &[true, true]);
        let result = h.update(&a, &p, 2, 256);
        assert!(result.is_none());
    }

    #[test]
    fn holds_last_active() {
        let mut h = LastMicHold::new(48000.0);
        // First: channel 1 is active
        let (a, p) = make_flags(&[false, true], &[true, true]);
        h.update(&a, &p, 2, 256);

        // Now: all silent — should hold channel 1
        let (a2, p2) = make_flags(&[false, false], &[true, true]);
        let result = h.update(&a2, &p2, 2, 256);
        assert_eq!(result, Some(1));
        assert!(h.is_holding());
    }

    #[test]
    fn hold_expires() {
        let mut h = LastMicHold::new(48000.0);
        // hold_duration at 48kHz, 500ms = 24000 samples

        // Activate channel 0
        let (a, p) = make_flags(&[true], &[true]);
        h.update(&a, &p, 1, 256);

        // Go silent and advance past hold duration
        let (a2, p2) = make_flags(&[false], &[true]);
        for _ in 0..200 {
            h.update(&a2, &p2, 1, 256);
        }
        // 200 * 256 = 51200 samples > 24000 — hold should have expired
        let result = h.update(&a2, &p2, 1, 256);
        assert!(result.is_none());
    }

    #[test]
    fn held_channel_muted_releases() {
        let mut h = LastMicHold::new(48000.0);
        // Activate channel 0
        let (a, p) = make_flags(&[true], &[true]);
        h.update(&a, &p, 1, 256);

        // Go silent, but channel 0 is no longer participating (muted)
        let (a2, p2) = make_flags(&[false], &[false]);
        let result = h.update(&a2, &p2, 1, 256);
        assert!(result.is_none());
    }

    #[test]
    fn hold_disabled_when_zero() {
        let mut h = LastMicHold::new(48000.0);
        h.set_hold_time_ms(0.0, 48000.0);

        // Activate then go silent
        let (a, p) = make_flags(&[true], &[true]);
        h.update(&a, &p, 1, 256);
        let (a2, p2) = make_flags(&[false], &[true]);
        let result = h.update(&a2, &p2, 1, 256);
        assert!(result.is_none());
    }

    #[test]
    fn retrigger_resets_counter() {
        let mut h = LastMicHold::new(48000.0);

        // Activate channel 0
        let (a, p) = make_flags(&[true, false], &[true, true]);
        h.update(&a, &p, 2, 256);

        // Go silent for a bit
        let (a_silent, p2) = make_flags(&[false, false], &[true, true]);
        for _ in 0..10 {
            h.update(&a_silent, &p2, 2, 256);
        }

        // Now channel 1 activates
        let (a3, p3) = make_flags(&[false, true], &[true, true]);
        h.update(&a3, &p3, 2, 256);

        // Go silent again — should hold channel 1 with reset counter
        let result = h.update(&a_silent, &p2, 2, 256);
        assert_eq!(result, Some(1));
    }
}
