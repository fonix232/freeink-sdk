#include "../../../src/driver/experimental/fast/PaperS3FastWaveform.h"

#include <cassert>
#include <initializer_list>

namespace waveform = freeink::papers3::fast;

int main() {
  // Source ordering is (new << 2) | old, intentionally unlike planner tables.
  constexpr uint8_t primary[] = {0, 1, 1, 1, 0, 0, 0, 1, 2, 0, 0, 0, 2, 2, 2, 0};
  constexpr uint8_t correction[] = {0, 0, 0, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 0, 2, 0};
  for (unsigned old = 0; old < 4; ++old) {
    for (unsigned next = 0; next < 4; ++next) {
      const auto index = next * 4 + old;
      for (size_t step = 0; step < 5; ++step) assert(waveform::drive(old, next, step) == primary[index]);
      assert(waveform::drive(old, next, 5) == correction[index]);
      assert(waveform::drive(old, next, 6) == 0);
      assert(waveform::drive(old, next, 7) == 0);
      assert(waveform::drive(old, next, SIZE_MAX) == 0);
    }
  }
  for (auto mode : {waveform::Mode::Differential, waveform::Mode::Full}) {
    for (unsigned old = 0; old < 256; ++old) {
      for (unsigned next = 0; next < 256; ++next) {
        for (size_t step = 0; step <= waveform::stepCount(mode); ++step) {
          const auto packed = waveform::driveByte(old, next, step, mode);
          for (unsigned pixel = 0; pixel < 4; ++pixel) {
            const unsigned shift = 6 - 2 * pixel;
            assert(((packed >> shift) & 3) ==
                   waveform::drive((old >> shift) & 3, (next >> shift) & 3, step, mode));
          }
        }
      }
    }
  }
  for (unsigned old = 0; old < 4; ++old) {
    for (unsigned next = 0; next < 4; ++next) {
      for (size_t step = 0; step < 26; ++step) {
        const auto code = waveform::drive(old, next, step, waveform::Mode::Full);
        if (step < 8) assert(code == 1);
        if (step >= 8 && step < 16) assert(code == 2);
        if (step == 16 || step == 25) assert(code == 0);
        if (step >= 17 && step < 25 && next == 0) assert(code == 1);
        if (step >= 17 && step < 25 && next == 3) assert(code == 2);
        assert(code <= 2);
      }
    }
  }
  assert(waveform::drive(4, 0, 0) == 0);
  assert(waveform::drive(0, 255, 0) == 0);
}
