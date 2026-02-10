use crate::engine::AutomixEngine;
use std::ffi::{c_char, c_float};

/// C-compatible channel metering struct.
#[repr(C)]
pub struct AutomixChannelMetering {
    pub input_rms_db: c_float,
    pub gain_db: c_float,
    pub output_rms_db: c_float,
    pub noise_floor_db: c_float,
    pub is_active: bool,
}

/// C-compatible global metering struct.
#[repr(C)]
pub struct AutomixGlobalMetering {
    pub nom_count: c_float,
    pub nom_attenuation_db: c_float,
}

// ---- Lifecycle ----

/// Create a new AutomixEngine instance.
/// Returns an opaque pointer that must be freed with `automix_destroy`.
///
/// # Safety
/// Caller must eventually pass the returned pointer to `automix_destroy`.
#[no_mangle]
pub unsafe extern "C" fn automix_create(
    num_channels: u32,
    sample_rate: c_float,
    _max_block_size: u32,
) -> *mut AutomixEngine {
    let engine = AutomixEngine::new(num_channels as usize, sample_rate);
    Box::into_raw(engine)
}

/// Destroy an AutomixEngine instance and free its memory.
///
/// # Safety
/// `engine` must be a pointer returned by `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_destroy(engine: *mut AutomixEngine) {
    if !engine.is_null() {
        drop(Box::from_raw(engine));
    }
}

// ---- Processing ----

/// Process a block of audio in-place.
/// `channel_ptrs`: array of `num_channels` pointers, each to `num_samples` f32 values.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`. `channel_ptrs` must
/// point to an array of at least `num_channels` valid pointers, each pointing
/// to at least `num_samples` f32 values. Null pointers are handled gracefully.
#[no_mangle]
pub unsafe extern "C" fn automix_process(
    engine: *mut AutomixEngine,
    channel_ptrs: *const *mut c_float,
    num_channels: u32,
    num_samples: u32,
) {
    if engine.is_null() || channel_ptrs.is_null() {
        return;
    }
    let engine = &mut *engine;
    engine.process_raw(channel_ptrs, num_channels as usize, num_samples as usize);
}

// ---- Version ----

/// Returns a pointer to a null-terminated version string.
#[no_mangle]
pub extern "C" fn automix_version() -> *const c_char {
    concat!(env!("CARGO_PKG_VERSION"), "\0").as_ptr() as *const c_char
}

// ---- Parameter Setters ----

/// Set the weight for a channel (linear, 0.0–1.0).
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_channel_weight(
    engine: *mut AutomixEngine,
    channel: u32,
    weight: c_float,
) {
    if engine.is_null() {
        return;
    }
    (*engine).set_channel_weight(channel as usize, weight as f64);
}

/// Set the mute state for a channel.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_channel_mute(
    engine: *mut AutomixEngine,
    channel: u32,
    muted: bool,
) {
    if engine.is_null() {
        return;
    }
    (*engine).set_channel_mute(channel as usize, muted);
}

/// Set the solo state for a channel.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_channel_solo(
    engine: *mut AutomixEngine,
    channel: u32,
    soloed: bool,
) {
    if engine.is_null() {
        return;
    }
    (*engine).set_channel_solo(channel as usize, soloed);
}

/// Set the bypass state for a channel.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_channel_bypass(
    engine: *mut AutomixEngine,
    channel: u32,
    bypassed: bool,
) {
    if engine.is_null() {
        return;
    }
    (*engine).set_channel_bypass(channel as usize, bypassed);
}

/// Set the global bypass state. When bypassed, audio passes through unmodified.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_global_bypass(engine: *mut AutomixEngine, bypass: bool) {
    if engine.is_null() {
        return;
    }
    (*engine).set_global_bypass(bypass);
}

/// Set the gain smoothing attack time in milliseconds.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_attack_ms(engine: *mut AutomixEngine, ms: c_float) {
    if engine.is_null() {
        return;
    }
    (*engine).set_attack_ms(ms as f64);
}

/// Set the gain smoothing release time in milliseconds.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_release_ms(engine: *mut AutomixEngine, ms: c_float) {
    if engine.is_null() {
        return;
    }
    (*engine).set_release_ms(ms as f64);
}

/// Set the last-mic-hold time in milliseconds.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_hold_time_ms(engine: *mut AutomixEngine, ms: c_float) {
    if engine.is_null() {
        return;
    }
    (*engine).set_hold_time_ms(ms as f64);
}

/// Enable or disable NOM attenuation.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
#[no_mangle]
pub unsafe extern "C" fn automix_set_nom_atten_enabled(engine: *mut AutomixEngine, enabled: bool) {
    if engine.is_null() {
        return;
    }
    (*engine).set_nom_atten_enabled(enabled);
}

// ---- Metering Getters ----

/// Get metering data for a single channel.
/// Returns true on success, false if engine is null or channel out of range.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
/// `out` must point to a valid `AutomixChannelMetering` struct, or be null.
#[no_mangle]
pub unsafe extern "C" fn automix_get_channel_metering(
    engine: *const AutomixEngine,
    channel: u32,
    out: *mut AutomixChannelMetering,
) -> bool {
    if engine.is_null() || out.is_null() {
        return false;
    }
    let engine = &*engine;
    if let Some(m) = engine.channel_metering(channel as usize) {
        (*out).input_rms_db = m.input_rms_db as c_float;
        (*out).gain_db = m.gain_db as c_float;
        (*out).output_rms_db = m.output_rms_db as c_float;
        (*out).noise_floor_db = m.noise_floor_db as c_float;
        (*out).is_active = m.is_active;
        true
    } else {
        false
    }
}

/// Get global metering data.
/// Returns true on success, false if engine or out pointer is null.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
/// `out` must point to a valid `AutomixGlobalMetering` struct, or be null.
#[no_mangle]
pub unsafe extern "C" fn automix_get_global_metering(
    engine: *const AutomixEngine,
    out: *mut AutomixGlobalMetering,
) -> bool {
    if engine.is_null() || out.is_null() {
        return false;
    }
    let engine = &*engine;
    let gm = engine.global_metering();
    (*out).nom_count = gm.nom_count as c_float;
    (*out).nom_attenuation_db = gm.nom_attenuation_db as c_float;
    true
}

/// Get metering data for all channels at once.
/// `out` must point to an array of at least `max_channels` `AutomixChannelMetering` structs.
/// Returns the number of channels written.
///
/// # Safety
/// `engine` must be a valid pointer from `automix_create`, or null.
/// `out` must point to an array of at least `max_channels` `AutomixChannelMetering` structs.
#[no_mangle]
pub unsafe extern "C" fn automix_get_all_channel_metering(
    engine: *const AutomixEngine,
    out: *mut AutomixChannelMetering,
    max_channels: u32,
) -> u32 {
    if engine.is_null() || out.is_null() {
        return 0;
    }
    let engine = &*engine;
    let n = (max_channels as usize).min(engine.num_channels());
    for i in 0..n {
        if let Some(m) = engine.channel_metering(i) {
            let dst = &mut *out.add(i);
            dst.input_rms_db = m.input_rms_db as c_float;
            dst.gain_db = m.gain_db as c_float;
            dst.output_rms_db = m.output_rms_db as c_float;
            dst.noise_floor_db = m.noise_floor_db as c_float;
            dst.is_active = m.is_active;
        }
    }
    n as u32
}
