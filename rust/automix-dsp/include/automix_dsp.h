#ifndef AUTOMIX_DSP_H
#define AUTOMIX_DSP_H

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Maximum number of channels supported.
#define AUTOMIX_MAX_CHANNELS 32

// Core automix engine (stub for Phase 0).
typedef struct AutomixEngine AutomixEngine;

// Create a new AutomixEngine instance.
// Returns an opaque pointer that must be freed with `automix_destroy`.
struct AutomixEngine *automix_create(uint32_t num_channels,
                                     float sample_rate,
                                     uint32_t _max_block_size);

// Destroy an AutomixEngine instance and free its memory.
void automix_destroy(struct AutomixEngine *engine);

// Process a block of audio in-place.
// `channel_ptrs`: array of `num_channels` pointers, each to `num_samples` f32 values.
// Phase 0: passthrough (audio is left unmodified).
void automix_process(struct AutomixEngine *engine,
                     float *const *channel_ptrs,
                     uint32_t num_channels,
                     uint32_t num_samples);

// Returns a pointer to a null-terminated version string.
const uint8_t *automix_version(void);

#endif  /* AUTOMIX_DSP_H */
