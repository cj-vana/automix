/// Maximum ring buffer capacity: 100ms at 192kHz.
const RING_BUFFER_MAX_CAPACITY: usize = 19200;

/// Ceiling on a stored squared sample.
///
/// A running sum only stays correct if the values in it are the same order of
/// magnitude. Push one enormous-but-finite sample and every subsequent small
/// value is swallowed by rounding; when the big one rotates out, the sum is
/// left with a permanent error, because `sum -= old; sum += new` preserves any
/// error it already has forever. A single loud thump could park a channel's
/// measured level wrong for the rest of the show.
///
/// 100.0 is +40 dBFS squared — far above anything a sane signal chain produces,
/// but low enough that 19200 of them sum without losing precision.
const MAX_SQUARED_SAMPLE: f64 = 100.0;

/// Fixed-capacity ring buffer that stores squared sample values and maintains
/// a running sum for O(1) RMS computation.
pub struct RingBuffer {
    buffer: [f64; RING_BUFFER_MAX_CAPACITY],
    write_pos: usize,
    window_len: usize,
    running_sum: f64,
    samples_written: u64,
}

impl RingBuffer {
    pub fn new(window_len: usize) -> Self {
        let window_len = window_len.clamp(1, RING_BUFFER_MAX_CAPACITY);
        Self {
            buffer: [0.0; RING_BUFFER_MAX_CAPACITY],
            write_pos: 0,
            window_len,
            running_sum: 0.0,
            samples_written: 0,
        }
    }

    /// Push a squared sample value into the buffer.
    #[inline]
    pub fn push(&mut self, squared_sample: f64) {
        // Reject non-finite values and clamp the magnitude, so the running sum
        // never mixes wildly different scales. See MAX_SQUARED_SAMPLE.
        let squared_sample = if squared_sample.is_finite() {
            squared_sample.clamp(0.0, MAX_SQUARED_SAMPLE)
        } else {
            0.0
        };

        // Subtract the oldest value that will be overwritten
        let old = self.buffer[self.write_pos];
        self.running_sum -= old;
        self.running_sum += squared_sample;

        // Guard against floating-point drift making the sum negative
        if self.running_sum < 0.0 {
            self.running_sum = 0.0;
        }

        self.buffer[self.write_pos] = squared_sample;
        self.write_pos += 1;
        if self.write_pos >= self.window_len {
            self.write_pos = 0;
            // Rebuild the sum exactly once per wrap. Any residual error in the
            // incremental sum is otherwise invariant under push() and would
            // never wash out. Amortised this is one extra add per sample.
            self.running_sum = self.buffer[..self.window_len].iter().sum();
        }
        self.samples_written += 1;
    }

    /// Mean of the values currently in the buffer.
    /// During partial fill, divides by actual number of samples written.
    #[inline]
    pub fn mean(&self) -> f64 {
        let count = if self.samples_written < self.window_len as u64 {
            self.samples_written as f64
        } else {
            self.window_len as f64
        };
        if count <= 0.0 {
            return 0.0;
        }
        self.running_sum / count
    }

    /// RMS value (square root of mean of squared samples).
    #[inline]
    pub fn rms(&self) -> f64 {
        self.mean().sqrt()
    }

    /// Reset the buffer to empty state, preserving window length.
    pub fn reset(&mut self) {
        self.buffer[..self.window_len].fill(0.0);
        self.write_pos = 0;
        self.running_sum = 0.0;
        self.samples_written = 0;
    }

    /// Change the window length. Resets the buffer.
    pub fn set_window_len(&mut self, window_len: usize) {
        self.window_len = window_len.clamp(1, RING_BUFFER_MAX_CAPACITY);
        self.reset();
    }

    #[inline]
    pub fn window_len(&self) -> usize {
        self.window_len
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    #[test]
    fn empty_buffer_rms_is_zero() {
        let rb = RingBuffer::new(10);
        assert_eq!(rb.rms(), 0.0);
    }

    #[test]
    fn single_sample() {
        let mut rb = RingBuffer::new(10);
        rb.push(4.0); // squared sample: mean = 4/1, rms = 2
        assert_relative_eq!(rb.mean(), 4.0, epsilon = 1e-10);
        assert_relative_eq!(rb.rms(), 2.0, epsilon = 1e-10);
    }

    #[test]
    fn partial_fill() {
        let mut rb = RingBuffer::new(4);
        rb.push(1.0);
        rb.push(1.0);
        // 2 samples written, sum = 2, mean = 2/2 = 1
        assert_relative_eq!(rb.mean(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn full_window() {
        let mut rb = RingBuffer::new(4);
        for _ in 0..4 {
            rb.push(1.0);
        }
        // mean = 4/4 = 1
        assert_relative_eq!(rb.mean(), 1.0, epsilon = 1e-10);
    }

    #[test]
    fn wrap_around() {
        let mut rb = RingBuffer::new(4);
        // Fill with 1.0
        for _ in 0..4 {
            rb.push(1.0);
        }
        // Now push 0.0 values to replace
        for _ in 0..4 {
            rb.push(0.0);
        }
        assert_relative_eq!(rb.mean(), 0.0, epsilon = 1e-10);
    }

    #[test]
    fn reset_clears_state() {
        let mut rb = RingBuffer::new(4);
        for _ in 0..10 {
            rb.push(5.0);
        }
        rb.reset();
        assert_eq!(rb.rms(), 0.0);
        assert_eq!(rb.mean(), 0.0);
    }

    #[test]
    fn dc_signal_rms() {
        // Push squared values of a DC signal at amplitude 0.5
        // squared = 0.25, RMS should be 0.5
        let mut rb = RingBuffer::new(100);
        for _ in 0..100 {
            rb.push(0.25); // 0.5^2
        }
        assert_relative_eq!(rb.rms(), 0.5, epsilon = 1e-10);
    }

    #[test]
    fn set_window_len_resets() {
        let mut rb = RingBuffer::new(10);
        for _ in 0..10 {
            rb.push(1.0);
        }
        rb.set_window_len(20);
        assert_eq!(rb.window_len(), 20);
        assert_eq!(rb.rms(), 0.0);
    }

    #[test]
    fn max_capacity_clamped() {
        let rb = RingBuffer::new(100_000);
        assert_eq!(rb.window_len(), RING_BUFFER_MAX_CAPACITY);
    }

    /// The old version asserted `mean() >= 0.0`, which push() clamps into
    /// existence — it could not fail. Compare against the exact sum instead.
    #[test]
    fn running_sum_matches_exact_sum_after_long_run() {
        let mut rb = RingBuffer::new(100);
        let mut recent = Vec::new();

        for i in 0..10_000 {
            let val = ((i % 10) as f64) * 0.01;
            rb.push(val);
            recent.push(val);
            if recent.len() > 100 {
                recent.remove(0);
            }
        }

        let exact: f64 = recent.iter().sum::<f64>() / 100.0;
        assert_relative_eq!(rb.mean(), exact, epsilon = 1e-12);
    }

    /// A single enormous sample used to leave a permanent error in the running
    /// sum, parking the channel's measured level wrong for good.
    #[test]
    fn recovers_from_a_huge_transient() {
        let mut rb = RingBuffer::new(64);

        for _ in 0..64 {
            rb.push(0.25);
        }
        assert_relative_eq!(rb.rms(), 0.5, epsilon = 1e-12);

        rb.push(1.0e30);

        // Flush the transient out of the window with a known level.
        for _ in 0..128 {
            rb.push(0.25);
        }

        assert_relative_eq!(rb.rms(), 0.5, epsilon = 1e-9);
    }

    #[test]
    fn huge_sample_is_clamped_not_stored_verbatim() {
        let mut rb = RingBuffer::new(8);
        rb.push(f64::MAX);
        assert!(rb.mean().is_finite());
        assert!(rb.mean() <= MAX_SQUARED_SAMPLE);
    }

    #[test]
    fn negative_squared_input_does_not_corrupt_the_sum() {
        let mut rb = RingBuffer::new(8);
        rb.push(-5.0);
        assert!(rb.mean() >= 0.0);
        for _ in 0..16 {
            rb.push(0.25);
        }
        assert_relative_eq!(rb.rms(), 0.5, epsilon = 1e-12);
    }
}
