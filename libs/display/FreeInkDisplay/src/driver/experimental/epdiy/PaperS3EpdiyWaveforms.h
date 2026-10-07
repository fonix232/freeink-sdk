// SPDX-License-Identifier: LGPL-3.0-only
// ED047TC1 waveform data adapted from EPDiy contributors; see README.md and LICENSE.
#pragma once

#include <cstddef>
#include <cstdint>

namespace freeink::papers3::epdiy {
// Four input tones: black, dark gray, light gray, white.
// Source codes: 0 = neutral, 1 = darken, 2 = lighten. No code remapping.
enum class Mode : uint8_t { DirectMono, Grayscale, Text };

struct Phase {
  // Two bits per transition, indexed by oldTone * 4 + newTone.
  uint32_t packedTransitions;
  // CKV active pulse, in 100 ns ticks. NOT an inter-frame delay.
  uint16_t gateHighTenthsUs;
  uint16_t gateLowTenthsUs;
};

inline constexpr uint32_t PIXEL_CLOCK_HZ = 20000000;
inline constexpr int MIN_TEMPERATURE_C = 20;
inline constexpr int MAX_TEMPERATURE_C = 30;
inline constexpr Phase NEUTRAL = {0, 100, 50};

inline constexpr Phase DIRECT_MONO[] = {
    {0x01818180u, 1000, 50},
    {0x01818180u, 1000, 50},
    {0x01818180u, 1000, 50},
    {0x01818180u, 1000, 50},
    {0x01818180u, 1000, 50},
    NEUTRAL,
};

inline constexpr Phase GRAYSCALE[] = {
    {0x55000000u, 30, 50},
    {0x55000000u, 30, 50},
    {0x55000000u, 20, 50},
    {0x55000000u, 20, 50},
    {0x55000000u, 30, 50},
    {0x55550000u, 30, 50},
    {0x55550000u, 30, 50},
    {0x55550000u, 40, 50},
    {0x55550000u, 40, 50},
    {0x55550000u, 50, 50},
    {0x55555500u, 50, 50},
    {0x55555500u, 50, 50},
    {0x55555500u, 100, 50},
    {0x55555500u, 200, 50},
    {0x55555500u, 300, 50},
    {0xa8a8a8a8u, 10, 50},
    {0xa8a8a8a8u, 10, 50},
    {0xa8a8a8a8u, 8, 50},
    {0xa8a8a8a8u, 8, 50},
    {0xa8a8a8a8u, 8, 50},
    {0xa0a0a0a0u, 8, 50},
    {0xa0a0a0a0u, 8, 50},
    {0xa0a0a0a0u, 10, 50},
    {0xa0a0a0a0u, 10, 50},
    {0xa0a0a0a0u, 15, 50},
    {0x80808080u, 15, 50},
    {0x80808080u, 20, 50},
    {0x80808080u, 20, 50},
    {0x80808080u, 100, 50},
    {0x80808080u, 300, 50},
    NEUTRAL,
};

inline constexpr Phase TEXT[] = {
    {0x15000000u, 30, 50},
    {0x15000000u, 30, 50},
    {0x15000000u, 20, 50},
    {0x15000000u, 20, 50},
    {0x15000000u, 30, 50},
    {0x15550000u, 30, 50},
    {0x15550000u, 30, 50},
    {0x15550000u, 40, 50},
    {0x15550000u, 40, 50},
    {0x15550000u, 50, 50},
    {0x15555500u, 50, 50},
    {0x15555500u, 50, 50},
    {0x15555500u, 100, 50},
    {0x15555500u, 200, 50},
    {0x15555500u, 300, 50},
    {0x28a8a8a8u, 10, 50},
    {0x28a8a8a8u, 10, 50},
    {0x28a8a8a8u, 8, 50},
    {0x28a8a8a8u, 8, 50},
    {0x28a8a8a8u, 8, 50},
    {0x20a0a0a0u, 8, 50},
    {0x20a0a0a0u, 8, 50},
    {0x20a0a0a0u, 10, 50},
    {0x20a0a0a0u, 10, 50},
    {0x20a0a0a0u, 15, 50},
    {0x00808080u, 15, 50},
    {0x00808080u, 20, 50},
    {0x00808080u, 20, 50},
    {0x00808080u, 100, 50},
    {0x00808080u, 300, 50},
    NEUTRAL,
};

constexpr size_t phaseCount(Mode mode) {
  switch (mode) {
    case Mode::DirectMono: return sizeof(DIRECT_MONO) / sizeof(DIRECT_MONO[0]);
    case Mode::Grayscale: return sizeof(GRAYSCALE) / sizeof(GRAYSCALE[0]);
    case Mode::Text: return sizeof(TEXT) / sizeof(TEXT[0]);
  }
  return 0;
}

constexpr Phase phase(Mode mode, size_t index) {
  if (index >= phaseCount(mode)) return NEUTRAL;
  switch (mode) {
    case Mode::DirectMono: return DIRECT_MONO[index];
    case Mode::Grayscale: return GRAYSCALE[index];
    case Mode::Text: return TEXT[index];
  }
  return NEUTRAL;
}

// PaperS3 presentation policy: endpoints always receive DU's full drive.
// For antialiased pages, paint intermediate gray destinations separately so
// black-to-white erasure is not weakened by the short GL16 gray timings.
constexpr size_t presentationPhaseCount(Mode mode) {
  return mode == Mode::DirectMono ? phaseCount(mode) : 5 + phaseCount(mode);
}
constexpr Phase presentationPhase(Mode mode, size_t index) {
  if (mode == Mode::DirectMono) return phase(mode, index);
  if (index < 5) return DIRECT_MONO[index];
  auto pulse = phase(mode, index - 5);
  pulse.packedTransitions &= 0x3c3c3c3cu; // Only dark/light gray destinations.
  return pulse;
}

constexpr uint8_t drive(uint8_t before, uint8_t after, Phase pulse) {
  if (before > 3 || after > 3) return 0;
  return (pulse.packedTransitions >> (2 * (before * 4 + after))) & 3;
}

constexpr uint8_t driveByte(uint8_t before, uint8_t after, Phase pulse) {
  uint8_t result = 0;
  for (unsigned shift = 0; shift < 8; shift += 2) {
    result |= drive((before >> shift) & 3, (after >> shift) & 3, pulse) << shift;
  }
  return result;
}

// The transport must reject unsupported gate timing before energizing the panel.
// Whole-microsecond GPIO delays cannot represent the 0.8 us phases faithfully.
template <typename Transport>
bool run(Mode mode, Transport& transport) {
  if (!phaseCount(mode)) return false;
  for (size_t i = 0; i < phaseCount(mode); ++i) {
    const auto pulse = phase(mode, i);
    if (!transport.supportsTiming(pulse.gateHighTenthsUs, pulse.gateLowTenthsUs)) return false;
  }
  for (size_t i = 0; i < phaseCount(mode); ++i) {
    if (!transport.scan(phase(mode, i))) {
      transport.abort();
      return false;
    }
  }
  return true;
}
}  // namespace freeink::papers3::epdiy
