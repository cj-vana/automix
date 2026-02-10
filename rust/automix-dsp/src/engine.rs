use crate::channel::Channel;
use crate::constants::{
    AUTOMIX_MAX_CHANNELS, DEFAULT_ATTACK_MS, DEFAULT_RELEASE_MS, MAX_ATTACK_MS, MAX_HOLD_TIME_MS,
    MAX_RELEASE_MS, MAX_WEIGHT, MIN_ATTACK_MS, MIN_HOLD_TIME_MS, MIN_RELEASE_MS, MIN_WEIGHT,
};
use crate::gain_sharing::compute_dugan_gains;
use crate::last_mic_hold::LastMicHold;
use crate::nom::NomAttenuation;

/// Global engine parameters.
struct EngineParams {
    global_bypass: bool,
    attack_ms: f64,
    release_ms: f64,
    sample_rate: f64,
}

/// Global metering snapshot.
#[derive(Debug, Clone, Copy, Default)]
pub struct GlobalMetering {
    pub nom_count: f64,
    pub nom_attenuation_db: f64,
}

/// Core automix engine implementing the 9-phase Dugan gain-sharing pipeline.
///
/// The channels array is Box-allocated to avoid stack overflow (~150KB per
/// channel * 32 channels = ~4.8MB).
pub struct AutomixEngine {
    num_channels: usize,
    channels: Box<[Channel; AUTOMIX_MAX_CHANNELS]>,
    params: EngineParams,
    last_mic_hold: LastMicHold,
    nom_atten: NomAttenuation,
    pub(crate) global_metering: GlobalMetering,
    sample_counter: u64,
    // Pre-allocated scratch arrays (no heap alloc in process path)
    rms_levels: [f64; AUTOMIX_MAX_CHANNELS],
    is_active: [bool; AUTOMIX_MAX_CHANNELS],
    participating: [bool; AUTOMIX_MAX_CHANNELS],
    weights: [f64; AUTOMIX_MAX_CHANNELS],
}

impl AutomixEngine {
    pub fn new(num_channels: usize, sample_rate: f32) -> Box<Self> {
        let sr = sample_rate as f64;
        let num_channels = num_channels.min(AUTOMIX_MAX_CHANNELS);

        // Heap-allocate channels individually, then collect into boxed array.
        // This avoids placing ~4.8MB of RingBuffers on the stack.
        let channels_vec: Vec<Channel> = (0..AUTOMIX_MAX_CHANNELS)
            .map(|i| Channel::new(i, sr))
            .collect();
        let channels: Box<[Channel; AUTOMIX_MAX_CHANNELS]> =
            channels_vec.into_boxed_slice().try_into().ok().unwrap();

        Box::new(Self {
            num_channels,
            channels,
            params: EngineParams {
                global_bypass: false,
                attack_ms: DEFAULT_ATTACK_MS,
                release_ms: DEFAULT_RELEASE_MS,
                sample_rate: sr,
            },
            last_mic_hold: LastMicHold::new(sr),
            nom_atten: NomAttenuation::new(),
            global_metering: GlobalMetering::default(),
            sample_counter: 0,
            rms_levels: [0.0; AUTOMIX_MAX_CHANNELS],
            is_active: [false; AUTOMIX_MAX_CHANNELS],
            participating: [false; AUTOMIX_MAX_CHANNELS],
            weights: [0.0; AUTOMIX_MAX_CHANNELS],
        })
    }

    pub fn version() -> &'static str {
        env!("CARGO_PKG_VERSION")
    }

    /// Process a block of audio in-place through the 9-phase Dugan pipeline.
    ///
    /// # Safety
    /// `channel_ptrs` must point to an array of at least `num_channels` valid
    /// pointers, each pointing to at least `num_samples` f32 values.
    pub unsafe fn process_raw(
        &mut self,
        channel_ptrs: *const *mut f32,
        num_channels: usize,
        num_samples: usize,
    ) {
        let num_ch = num_channels.min(self.num_channels);
        if num_ch == 0 || num_samples == 0 {
            return;
        }

        // Global bypass: leave audio unmodified
        if self.params.global_bypass {
            return;
        }

        // Build per-channel slices from raw pointers
        let channel_ptrs_slice = std::slice::from_raw_parts(channel_ptrs, num_ch);

        // --- Phase 0: Determine participation flags ---
        let any_solo = self.channels[..num_ch].iter().any(|ch| ch.params.soloed);

        for i in 0..num_ch {
            self.participating[i] = self.channels[i].params.is_participating(any_solo);
            self.weights[i] = self.channels[i].params.weight;
        }

        // --- Phase 1: RMS level detection ---
        #[allow(clippy::needless_range_loop)]
        for i in 0..num_ch {
            let samples = std::slice::from_raw_parts(channel_ptrs_slice[i], num_samples);
            self.rms_levels[i] = self.channels[i].level_detector.process_block(samples);
        }

        // --- Phase 2: Noise floor tracking ---
        for i in 0..num_ch {
            if self.participating[i] {
                self.channels[i].noise_floor.update(self.rms_levels[i]);
            }
        }

        // --- Phase 3: Active channel detection ---
        for i in 0..num_ch {
            self.is_active[i] = if self.participating[i] {
                self.channels[i].noise_floor.is_active(self.rms_levels[i])
            } else {
                false
            };
            self.channels[i].is_active = self.is_active[i];
        }

        // --- Phase 4: Last-mic-hold evaluation ---
        let hold_channel =
            self.last_mic_hold
                .update(&self.is_active, &self.participating, num_ch, num_samples);

        // --- Phase 5: Dugan gain-sharing ---
        let result = compute_dugan_gains(
            &self.rms_levels,
            &self.weights,
            &self.is_active,
            &self.participating,
            num_ch,
            hold_channel,
        );

        // --- Phase 6: NOM attenuation ---
        self.nom_atten.update(result.nom);
        let nom_linear = self.nom_atten.linear();

        // --- Phase 7+8: Per-sample gain smoothing and application ---
        // Compute target gain per channel, then apply the one-pole smoother
        // per-sample while writing the gain-adjusted audio.
        #[allow(clippy::needless_range_loop)]
        for i in 0..num_ch {
            let target_gain = if self.participating[i] {
                result.gains[i] * nom_linear
            } else if self.channels[i].params.bypassed {
                1.0
            } else {
                0.0 // muted or excluded
            };

            self.channels[i].raw_gain = target_gain;

            if self.channels[i].params.bypassed {
                // Bypassed: set smoother to unity immediately, no per-sample work
                self.channels[i].gain_smoother.set_immediate(1.0);
                self.channels[i].smoothed_gain = 1.0;
                // Audio passes through unmodified
            } else {
                let buf = std::slice::from_raw_parts_mut(channel_ptrs_slice[i], num_samples);
                for sample in buf.iter_mut() {
                    let s = if sample.is_finite() { *sample } else { 0.0 };
                    let gain = self.channels[i].gain_smoother.process(target_gain);
                    *sample = s * gain as f32;
                }
                self.channels[i].smoothed_gain = self.channels[i].gain_smoother.current();
            }
        }

        // --- Phase 9: Update counters + metering snapshots ---
        self.sample_counter += num_samples as u64;

        for i in 0..num_ch {
            self.channels[i].update_metering(self.rms_levels[i]);
        }

        self.global_metering.nom_count = result.nom;
        self.global_metering.nom_attenuation_db = self.nom_atten.db();
    }

    // ---- Parameter setters ----

    pub fn set_channel_weight(&mut self, channel: usize, weight: f64) {
        if channel < self.num_channels {
            self.channels[channel].params.weight = weight.clamp(MIN_WEIGHT, MAX_WEIGHT);
        }
    }

    pub fn set_channel_mute(&mut self, channel: usize, muted: bool) {
        if channel < self.num_channels {
            self.channels[channel].params.muted = muted;
        }
    }

    pub fn set_channel_solo(&mut self, channel: usize, soloed: bool) {
        if channel < self.num_channels {
            self.channels[channel].params.soloed = soloed;
        }
    }

    pub fn set_channel_bypass(&mut self, channel: usize, bypassed: bool) {
        if channel < self.num_channels {
            self.channels[channel].params.bypassed = bypassed;
        }
    }

    pub fn set_global_bypass(&mut self, bypass: bool) {
        self.params.global_bypass = bypass;
    }

    pub fn set_attack_ms(&mut self, ms: f64) {
        let ms = ms.clamp(MIN_ATTACK_MS, MAX_ATTACK_MS);
        self.params.attack_ms = ms;
        let sr = self.params.sample_rate;
        let release = self.params.release_ms;
        for i in 0..self.num_channels {
            self.channels[i].set_smoothing(ms, release, sr);
        }
    }

    pub fn set_release_ms(&mut self, ms: f64) {
        let ms = ms.clamp(MIN_RELEASE_MS, MAX_RELEASE_MS);
        self.params.release_ms = ms;
        let sr = self.params.sample_rate;
        let attack = self.params.attack_ms;
        for i in 0..self.num_channels {
            self.channels[i].set_smoothing(attack, ms, sr);
        }
    }

    pub fn set_hold_time_ms(&mut self, ms: f64) {
        let ms = ms.clamp(MIN_HOLD_TIME_MS, MAX_HOLD_TIME_MS);
        self.last_mic_hold
            .set_hold_time_ms(ms, self.params.sample_rate);
    }

    pub fn set_nom_atten_enabled(&mut self, enabled: bool) {
        self.nom_atten.set_enabled(enabled);
    }

    // ---- Metering getters ----

    pub fn channel_metering(&self, channel: usize) -> Option<&crate::channel::ChannelMetering> {
        if channel < self.num_channels {
            Some(&self.channels[channel].metering)
        } else {
            None
        }
    }

    pub fn global_metering(&self) -> &GlobalMetering {
        &self.global_metering
    }

    pub fn num_channels(&self) -> usize {
        self.num_channels
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use approx::assert_relative_eq;

    /// Helper: create engine and process a block of audio.
    unsafe fn process_test_block(
        engine: &mut AutomixEngine,
        buffers: &mut [Vec<f32>],
        num_samples: usize,
    ) {
        let mut ptrs: Vec<*mut f32> = buffers.iter_mut().map(|b| b.as_mut_ptr()).collect();
        engine.process_raw(
            ptrs.as_mut_ptr() as *const *mut f32,
            buffers.len(),
            num_samples,
        );
    }

    #[test]
    fn global_bypass_passthrough() {
        let mut engine = AutomixEngine::new(2, 48000.0);
        engine.set_global_bypass(true);

        let mut buffers = vec![vec![0.5_f32; 256], vec![0.3_f32; 256]];

        unsafe { process_test_block(&mut engine, &mut buffers, 256) };

        // Audio should be unmodified
        assert_eq!(buffers[0], vec![0.5_f32; 256]);
        assert_eq!(buffers[1], vec![0.3_f32; 256]);
    }

    #[test]
    fn single_channel_gain_approaches_unity() {
        let mut engine = AutomixEngine::new(1, 48000.0);
        let amplitude = 0.5_f32;

        // Process several blocks to let smoothing converge
        for _ in 0..200 {
            let mut buffers = vec![vec![amplitude; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        // After convergence, the single active channel should have gain near 1.0
        // (with NOM=1 attenuation = 0dB)
        let mut final_buf = vec![vec![amplitude; 256]];
        unsafe { process_test_block(&mut engine, &mut final_buf, 256) };

        // Last sample should be close to original amplitude (gain ≈ 1.0)
        let last = final_buf[0][255];
        assert!(
            (last - amplitude).abs() < 0.05,
            "Expected ~{amplitude}, got {last}"
        );
    }

    #[test]
    fn louder_channel_gets_more_gain() {
        let mut engine = AutomixEngine::new(2, 48000.0);

        // Process many blocks: channel 0 loud, channel 1 quiet
        for _ in 0..200 {
            let mut buffers = vec![vec![0.8_f32; 256], vec![0.1_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        // Check final block
        let mut final_buf = vec![vec![0.8_f32; 256], vec![0.1_f32; 256]];
        unsafe { process_test_block(&mut engine, &mut final_buf, 256) };

        let ch0_out = final_buf[0][255].abs();
        let ch1_out = final_buf[1][255].abs();
        assert!(
            ch0_out > ch1_out,
            "Louder channel should have higher output: ch0={ch0_out}, ch1={ch1_out}"
        );
    }

    #[test]
    fn muted_channel_silenced() {
        let mut engine = AutomixEngine::new(2, 48000.0);
        engine.set_channel_mute(1, true);

        // Converge
        for _ in 0..200 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.5_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        let mut final_buf = vec![vec![0.5_f32; 256], vec![0.5_f32; 256]];
        unsafe { process_test_block(&mut engine, &mut final_buf, 256) };

        // Muted channel output should be near zero
        let muted_out = final_buf[1][255].abs();
        assert!(
            muted_out < 0.01,
            "Muted channel should be near zero: {muted_out}"
        );
    }

    #[test]
    fn bypassed_channel_unity() {
        let mut engine = AutomixEngine::new(2, 48000.0);
        engine.set_channel_bypass(0, true);

        // Converge
        for _ in 0..50 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.5_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        let mut final_buf = vec![vec![0.5_f32; 256], vec![0.5_f32; 256]];
        unsafe { process_test_block(&mut engine, &mut final_buf, 256) };

        // Bypassed channel should pass through at unity
        let bypassed_out = final_buf[0][255];
        assert_relative_eq!(bypassed_out, 0.5, epsilon = 0.01);
    }

    #[test]
    fn metering_after_process() {
        let mut engine = AutomixEngine::new(2, 48000.0);

        for _ in 0..50 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.1_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        let m = engine.channel_metering(0).unwrap();
        // Input RMS should be around -6dB for 0.5 amplitude
        assert!(m.input_rms_db > -10.0 && m.input_rms_db < 0.0);

        let gm = engine.global_metering();
        assert!(gm.nom_count >= 0.0);
    }

    #[test]
    fn version_string() {
        let v = AutomixEngine::version();
        assert!(!v.is_empty());
    }

    #[test]
    fn nan_input_produces_finite_output() {
        let mut engine = AutomixEngine::new(2, 48000.0);

        // First converge with normal audio
        for _ in 0..100 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.3_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        // Inject NaN
        let mut nan_buf = vec![vec![f32::NAN; 256], vec![0.3_f32; 256]];
        unsafe { process_test_block(&mut engine, &mut nan_buf, 256) };

        // All outputs should be finite
        for ch in &nan_buf {
            for &s in ch {
                assert!(
                    s.is_finite(),
                    "Output sample is not finite after NaN injection"
                );
            }
        }

        // Engine should recover: process more normal audio
        for _ in 0..100 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.3_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
            for ch in &buffers {
                for &s in ch {
                    assert!(s.is_finite(), "Output sample is not finite after recovery");
                }
            }
        }
    }

    #[test]
    fn inf_input_produces_finite_output() {
        let mut engine = AutomixEngine::new(2, 48000.0);

        // Converge
        for _ in 0..100 {
            let mut buffers = vec![vec![0.5_f32; 256], vec![0.3_f32; 256]];
            unsafe { process_test_block(&mut engine, &mut buffers, 256) };
        }

        // Inject Inf and -Inf
        let mut inf_buf = vec![vec![f32::INFINITY; 256], vec![f32::NEG_INFINITY; 256]];
        unsafe { process_test_block(&mut engine, &mut inf_buf, 256) };

        for ch in &inf_buf {
            for &s in ch {
                assert!(
                    s.is_finite(),
                    "Output sample is not finite after Inf injection"
                );
            }
        }
    }
}
