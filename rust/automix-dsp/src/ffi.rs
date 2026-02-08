use crate::AutomixEngine;
use std::ffi::c_float;

/// Create a new AutomixEngine instance.
/// Returns an opaque pointer that must be freed with `automix_destroy`.
#[no_mangle]
pub unsafe extern "C" fn automix_create(
    num_channels: u32,
    sample_rate: c_float,
    _max_block_size: u32,
) -> *mut AutomixEngine {
    let engine = Box::new(AutomixEngine::new(num_channels as usize, sample_rate));
    Box::into_raw(engine)
}

/// Destroy an AutomixEngine instance and free its memory.
#[no_mangle]
pub unsafe extern "C" fn automix_destroy(engine: *mut AutomixEngine) {
    if !engine.is_null() {
        drop(Box::from_raw(engine));
    }
}

/// Process a block of audio in-place.
/// `channel_ptrs`: array of `num_channels` pointers, each to `num_samples` f32 values.
/// Phase 0: passthrough (audio is left unmodified).
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

/// Returns a pointer to a null-terminated version string.
#[no_mangle]
pub extern "C" fn automix_version() -> *const u8 {
    b"0.1.0\0".as_ptr()
}
