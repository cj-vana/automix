#ifndef AUTOMIX_DSP_H
#define AUTOMIX_DSP_H

#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Maximum number of channels the engine supports.
#define AUTOMIX_MAX_CHANNELS 32

// Maximum block size in samples.
#define AUTOMIX_MAX_BLOCK_SIZE 4096

// Core automix engine implementing the 9-phase Dugan gain-sharing pipeline.
//
// The channels array is Box-allocated to avoid stack overflow (~150KB per
// channel * 32 channels = ~4.8MB).
typedef struct AutomixEngine AutomixEngine;

// C-compatible channel metering struct.
typedef struct AutomixChannelMetering {
  float input_rms_db;
  float gain_db;
  float output_rms_db;
  float noise_floor_db;
  bool is_active;
} AutomixChannelMetering;

// C-compatible global metering struct.
typedef struct AutomixGlobalMetering {
  float nom_count;
  float nom_attenuation_db;
} AutomixGlobalMetering;

// Create a new AutomixEngine instance.
// Returns an opaque pointer that must be freed with `automix_destroy`.
struct AutomixEngine *automix_create(uint32_t num_channels,
                                     float sample_rate,
                                     uint32_t _max_block_size);

// Destroy an AutomixEngine instance and free its memory.
void automix_destroy(struct AutomixEngine *engine);

// Process a block of audio in-place.
// `channel_ptrs`: array of `num_channels` pointers, each to `num_samples` f32 values.
void automix_process(struct AutomixEngine *engine,
                     float *const *channel_ptrs,
                     uint32_t num_channels,
                     uint32_t num_samples);

// Returns a pointer to a null-terminated version string.
const uint8_t *automix_version(void);

// Set the weight for a channel (linear, 0.0–1.0).
void automix_set_channel_weight(struct AutomixEngine *engine, uint32_t channel, float weight);

// Set the mute state for a channel.
void automix_set_channel_mute(struct AutomixEngine *engine, uint32_t channel, bool muted);

// Set the solo state for a channel.
void automix_set_channel_solo(struct AutomixEngine *engine, uint32_t channel, bool soloed);

// Set the bypass state for a channel.
void automix_set_channel_bypass(struct AutomixEngine *engine, uint32_t channel, bool bypassed);

// Set the global bypass state. When bypassed, audio passes through unmodified.
void automix_set_global_bypass(struct AutomixEngine *engine, bool bypass);

// Set the gain smoothing attack time in milliseconds.
void automix_set_attack_ms(struct AutomixEngine *engine, float ms);

// Set the gain smoothing release time in milliseconds.
void automix_set_release_ms(struct AutomixEngine *engine, float ms);

// Set the last-mic-hold time in milliseconds.
void automix_set_hold_time_ms(struct AutomixEngine *engine, float ms);

// Enable or disable NOM attenuation.
void automix_set_nom_atten_enabled(struct AutomixEngine *engine, bool enabled);

// Get metering data for a single channel.
// Returns true on success, false if engine is null or channel out of range.
bool automix_get_channel_metering(const struct AutomixEngine *engine,
                                  uint32_t channel,
                                  struct AutomixChannelMetering *out);

// Get global metering data.
// Returns true on success, false if engine or out pointer is null.
bool automix_get_global_metering(const struct AutomixEngine *engine,
                                 struct AutomixGlobalMetering *out);

// Get metering data for all channels at once.
// `out` must point to an array of at least `max_channels` `AutomixChannelMetering` structs.
// Returns the number of channels written.
uint32_t automix_get_all_channel_metering(const struct AutomixEngine *engine,
                                          struct AutomixChannelMetering *out,
                                          uint32_t max_channels);

#endif  /* AUTOMIX_DSP_H */
