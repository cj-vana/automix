use crate::math_utils::{linear_to_db, ms_to_samples};
use crate::ring_buffer::RingBuffer;

/// RMS level detector using a sliding-window ring buffer.
pub struct LevelDetector {
    ring_buffer: RingBuffer,
    current_rms: f64,
}

impl LevelDetector {
    pub fn new(window_ms: f64, sample_rate: f64) -> Self {
        let window_samples = ms_to_samples(window_ms, sample_rate).max(1);
        Self {
            ring_buffer: RingBuffer::new(window_samples),
            current_rms: 0.0,
        }
    }

    /// Process a single sample (f32 from the audio buffer).
    #[inline]
    pub fn process_sample(&mut self, sample: f32) {
        let s = if sample.is_finite() {
            sample as f64
        } else {
            0.0
        };
        self.ring_buffer.push(s * s);
    }

    /// Process a block of samples. Returns the RMS at the end of the block.
    pub fn process_block(&mut self, samples: &[f32]) -> f64 {
        for &s in samples {
            let sd = if s.is_finite() { s as f64 } else { 0.0 };
            self.ring_buffer.push(sd * sd);
        }
        self.current_rms = self.ring_buffer.rms();
        self.current_rms
    }

    /// Current RMS level (linear).
    #[inline]
    pub fn rms(&self) -> f64 {
        self.current_rms
    }

    /// Current RMS level in dB.
    #[inline]
    pub fn rms_db(&self) -> f64 {
        linear_to_db(self.current_rms)
    }

    /// Reset the detector state.
    pub fn reset(&mut self) {
        self.ring_buffer.reset();
        self.current_rms = 0.0;
    }

    /// Reconfigure the window size. Resets state.
    pub fn set_window(&mut self, window_ms: f64, sample_rate: f64) {
        let window_samples = ms_to_samples(window_ms, sample_rate).max(1);
        self.ring_buffer.set_window_len(window_samples);
        self.current_rms = 0.0;
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    #[test]
    fn silence_is_zero() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        let silence = vec![0.0_f32; 960];
        det.process_block(&silence);
        assert_eq!(det.rms(), 0.0);
    }

    #[test]
    fn dc_signal() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        // DC at amplitude 0.5 for full window (960 samples at 48kHz, 20ms)
        let dc = vec![0.5_f32; 960];
        det.process_block(&dc);
        assert_relative_eq!(det.rms(), 0.5, epsilon = 1e-6);
    }

    #[test]
    fn sine_wave_rms() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        let peak = 1.0_f32;
        let freq = 1000.0;
        let sr = 48000.0;
        // Generate enough samples to fill the window
        let samples: Vec<f32> = (0..4800)
            .map(|i| peak * (2.0 * std::f32::consts::PI * freq * i as f32 / sr as f32).sin())
            .collect();
        det.process_block(&samples);
        // RMS of sine = peak / sqrt(2) ≈ 0.7071
        assert_relative_eq!(det.rms(), 1.0 / 2.0_f64.sqrt(), epsilon = 0.01);
    }

    #[test]
    fn partial_window_startup() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        // Just push a few samples — should still give meaningful RMS
        let samples = vec![1.0_f32; 10];
        det.process_block(&samples);
        assert_relative_eq!(det.rms(), 1.0, epsilon = 1e-6);
    }

    #[test]
    fn rms_db_for_unity() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        let dc = vec![1.0_f32; 960];
        det.process_block(&dc);
        assert_relative_eq!(det.rms_db(), 0.0, epsilon = 0.01);
    }

    #[test]
    fn reset_clears() {
        let mut det = LevelDetector::new(20.0, 48000.0);
        let dc = vec![1.0_f32; 960];
        det.process_block(&dc);
        det.reset();
        assert_eq!(det.rms(), 0.0);
    }
}
