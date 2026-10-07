#pragma once

#include <cstddef>
#include <cstdint>

// Default PaperS3 four-tone waveform: per-transition pulse trains, all
// unchanged pixels floating. Waveform data adapted from Tony Weston /
// EPD_Painter, commit 6335a149c5a9b3be4150880e0be64ef2529ba62a. Apache-2.0, see
// PaperS3-EPDPainter-LICENSE.txt and PaperS3-EPDPainter-NOTICE.txt. Modified:
// native tone order, independent transition planner, float normalization and
// transport contract; no EPD_Painter runtime dependency.
namespace freeink::papers3::native {
inline constexpr size_t BASE_STEPS = 13;
inline constexpr size_t MAX_ACTIVE_STEPS = 26;
inline constexpr size_t STEPS = MAX_ACTIVE_STEPS + 1;
inline constexpr uint32_t POST_SCAN_GAP_US = 4000;

// Table levels (EPD_Painter order): 0 white, 1 light gray, 2 dark gray, 3 black.
// Electrical codes: 0 float, 1 darken, 2 whiten. Inherited code 3 is floated.
inline constexpr uint8_t REMOVE[3][BASE_STEPS] = {
    {1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2},
    {3, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2},
    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
};
inline constexpr uint8_t APPLY[3][BASE_STEPS] = {
    {1, 1, 1, 3, 1, 1, 1, 0, 0, 1, 2, 3, 2},
    {1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

struct Train {
  uint8_t count;
  uint8_t codes[17];
};
inline constexpr Train DIRECT[] = {
    {7, {2, 2, 2, 1, 1, 1, 1}},
    {14, {2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {13, {2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
    {14, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1}},
    {17, {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1}},
};
constexpr int directIndex(uint8_t from, uint8_t to) {
  if (from == 1 && to == 2) return 0;
  if (from == 1 && to == 3) return 1;
  if (from == 2 && to == 3) return 2;
  if (from == 3 && to == 1) return 3;
  if (from == 3 && to == 2) return 4;
  return -1;
}

// Inputs match PaperS3Driver's packed framebuffer: 0 black through 3 white.
constexpr size_t stepCount(uint8_t before, uint8_t after) {
  if (before > 3 || after > 3 || before == after) return 0;
  const uint8_t from = 3 - before, to = 3 - after;
  const int direct = directIndex(from, to);
  if (direct >= 0) return DIRECT[direct].count;
  return from != 0 && to != 0 ? BASE_STEPS * 2 : BASE_STEPS;
}

constexpr uint8_t drive(uint8_t before, uint8_t after, size_t step) {
  if (step >= stepCount(before, after)) return 0;
  const uint8_t from = 3 - before, to = 3 - after;
  const int direct = directIndex(from, to);
  uint8_t code = 0;
  if (direct >= 0) {
    code = DIRECT[direct].codes[step];
  } else if (from != 0 && step < BASE_STEPS) {
    code = REMOVE[from - 1][step];
  } else {
    code = APPLY[to - 1][step - (from != 0 ? BASE_STEPS : 0)];
  }
  return code == 3 ? 0 : code;
}

constexpr uint8_t driveByte(uint8_t before, uint8_t after, size_t step) {
  uint8_t result = 0;
  for (unsigned pixel = 0; pixel < 4; ++pixel) {
    const unsigned shift = 6 - pixel * 2;
    result |= drive((before >> shift) & 3, (after >> shift) & 3, step) << shift;
  }
  return result;
}
}  // namespace freeink::papers3::native
