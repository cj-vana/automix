/// Maximum ring buffer capacity: 100ms at 192kHz.
const RING_BUFFER_MAX_CAPACITY: usize = 19200;

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
        let window_len = window_len.min(RING_BUFFER_MAX_CAPACITY).max(1);
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
        self.window_len = window_len.min(RING_BUFFER_MAX_CAPACITY).max(1);
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

    #[test]
    fn numerical_stability_long_run() {
        let mut rb = RingBuffer::new(100);
        // Push many samples, then check sum doesn't drift negative
        for i in 0..10_000 {
            let val = ((i % 10) as f64) * 0.01;
            rb.push(val);
        }
        assert!(rb.mean() >= 0.0);
    }
}
