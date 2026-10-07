#pragma once

#include <cstddef>
#include <cstdint>

// Adapted from FastEPD, Copyright 2024 BitBank Software, Inc.
// Apache-2.0; see LICENSE.txt. Modified: PaperS3-only, four-tone, allocation-free
// waveform planner with caller-owned transport. See README.md for provenance.
namespace freeink::papers3::fast {

enum class Mode : uint8_t { Differential, Full };

inline constexpr uint32_t REFERENCE_CLOCK_HZ = 40000000;
inline constexpr uint16_t INTER_SCAN_PAUSE_US = 230;
inline constexpr size_t DIFFERENTIAL_STEPS = 7;
inline constexpr size_t FULL_STEPS = 26;

// Indexed by old tone then new tone; 0=black, 1=dark, 2=light, 3=white.
// Codes: 0=neutral, 1=darken, 2=lighten. FastEPD leaves unchanged pixels neutral.
inline constexpr uint8_t PRIMARY[4][4] = {
    {0, 0, 2, 2},
    {1, 0, 0, 2},
    {1, 0, 0, 2},
    {1, 1, 0, 0},
};
inline constexpr uint8_t CORRECTION[4][4] = {
    {0, 2, 1, 0},
    {0, 0, 2, 0},
    {0, 1, 0, 2},
    {0, 2, 1, 0},
};

// PaperS3 u8M5Matrix rows 0, 5, 10, 15, written from a white baseline.
inline constexpr uint8_t FROM_WHITE[4][8] = {
    {1, 1, 1, 1, 1, 1, 1, 1},
    {2, 2, 1, 1, 1, 2, 2, 1},
    {2, 2, 1, 1, 1, 1, 2, 2},
    {2, 2, 2, 2, 2, 2, 2, 2},
};

constexpr size_t stepCount(Mode mode) { return mode == Mode::Full ? FULL_STEPS : DIFFERENTIAL_STEPS; }

constexpr uint8_t drive(uint8_t before, uint8_t after, size_t step, Mode mode = Mode::Differential) {
  if (before > 3 || after > 3 || step >= stepCount(mode)) return 0;
  if (mode == Mode::Differential) {
    if (step < 5) return PRIMARY[before][after];
    return step == 5 ? CORRECTION[before][after] : 0;
  }
  // Full refresh establishes a baseline even when previous pixels are unknown.
  if (step < 8) return 1;
  if (step < 16) return 2;
  if (step == 16 || step == 25) return 0;
  return FROM_WHITE[after][step - 17];
}

// Packed pixels and drive codes both run most-significant pair first.
constexpr uint8_t driveByte(uint8_t before, uint8_t after, size_t step, Mode mode = Mode::Differential) {
  uint8_t result = 0;
  for (unsigned pixel = 0; pixel < 4; ++pixel) {
    const unsigned shift = 6 - pixel * 2;
    result |= drive((before >> shift) & 3, (after >> shift) & 3, step, mode) << shift;
  }
  return result;
}

}  // namespace freeink::papers3::fast
