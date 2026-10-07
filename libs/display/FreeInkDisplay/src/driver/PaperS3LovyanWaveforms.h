#pragma once
#include <cstddef>
#include <cstdint>

// Reference PaperS3 waveform: the drive tables from LovyanGFX Panel_EPD.cpp,
// bundled in M5GFX 0.2.29, kept to compare against the native waveform.
// Copyright (c) lovyan03. BSD-2-Clause; see PaperS3-LovyanGFX-LICENSE.txt.
// Transport, framebuffer ownership and execution are independent of LovyanGFX.
namespace freeink::papers3::lovyan {
constexpr uint32_t levels(unsigned a0, unsigned a1, unsigned a2, unsigned a3, unsigned a4, unsigned a5, unsigned a6,
                          unsigned a7, unsigned a8, unsigned a9, unsigned a10, unsigned a11, unsigned a12, unsigned a13,
                          unsigned a14, unsigned a15) {
  return a0 | (a1 << 2) | (a2 << 4) | (a3 << 6) | (a4 << 8) | (a5 << 10) | (a6 << 12) | (a7 << 14) | (a8 << 16) |
         (a9 << 18) | (a10 << 20) | (a11 << 22) | (a12 << 24) | (a13 << 26) | (a14 << 28) | (a15 << 30);
}
inline constexpr uint32_t ERASE[] = {
    levels(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 1, 1),
    levels(2, 2, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1),
    ~0u,
    0u,
};
inline constexpr uint32_t TEXT[] = {
    levels(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1),
    levels(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1),
    levels(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1),
    levels(2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 1),
    levels(2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 1),
    levels(1, 2, 2, 1, 1, 1, 1, 1, 3, 3, 1, 1, 3, 3, 1, 2),
    levels(1, 3, 3, 1, 1, 1, 1, 3, 3, 1, 1, 1, 1, 3, 1, 2),
    levels(1, 3, 3, 1, 2, 2, 1, 1, 1, 1, 2, 1, 1, 1, 1, 2),
    levels(3, 1, 3, 2, 2, 2, 1, 1, 1, 2, 2, 1, 1, 1, 2, 3),
    levels(1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
    levels(1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2),
    levels(1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 3, 3, 2),
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    ~0u,
    0u,
};
inline constexpr uint32_t FAST[] = {
    levels(2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1),
    levels(2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    levels(1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2),
    ~0u,
    0u,
};
// Four packed four-tone pixels use levels 0, 5, 10 and 15 of the panel table.
inline uint8_t tone(uint8_t pixels, unsigned pixel) { return (pixels >> (6 - 2 * pixel)) & 3; }
inline uint8_t drive(uint32_t phase, uint8_t tone) { return (phase >> (tone * 10)) & 3; }
inline constexpr size_t TEXT_ACTIVE_STEPS = 12;
inline constexpr size_t DIFFERENTIAL_SETTLE_STEPS = 12;
inline constexpr size_t DIFFERENTIAL_STEPS =
    sizeof(ERASE) / sizeof(ERASE[0]) + TEXT_ACTIVE_STEPS + DIFFERENTIAL_SETTLE_STEPS + 1;
inline uint8_t differentialDrive(uint8_t before, uint8_t after, size_t step) {
  if (step >= DIFFERENTIAL_STEPS - 1) return 0;
  if (before == after) return 3;
  // Monochrome transitions retain the same pulse train as non-AA page turns.
  if ((before == 0 || before == 3) && (after == 0 || after == 3))
    return step < sizeof(FAST) / sizeof(FAST[0]) ? drive(FAST[step], after) : 0;
  constexpr size_t eraseSteps = sizeof(ERASE) / sizeof(ERASE[0]);
  return step < eraseSteps ? drive(ERASE[step], before) : drive(TEXT[step - eraseSteps], after);
}
inline uint8_t driveByte(uint8_t before, uint8_t after, uint32_t phase, bool erase, bool force) {
  uint8_t out = 0;
  for (unsigned p = 0; p < 4; ++p) {
    const auto old = tone(before, p), next = tone(after, p);
    const uint8_t code = (!force && old == next) ? 3 : drive(phase, erase ? old : next);
    out |= code << (6 - 2 * p);
  }
  return out;
}
}  // namespace freeink::papers3::lovyan
