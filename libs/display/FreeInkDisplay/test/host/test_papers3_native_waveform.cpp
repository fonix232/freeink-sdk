#include "driver/PaperS3NativeWaveform.h"

#include <cassert>
#include <cstdio>

using namespace freeink::papers3::native;

int main() {
  constexpr int potential[] = {13, 6, 5, 0};
  for (uint8_t from = 0; from < 4; ++from) {
    for (uint8_t to = 0; to < 4; ++to) {
      int charge = 0;
      for (size_t step = 0; step < STEPS; ++step) {
        const auto code = drive(from, to, step);
        assert(code <= 2);
        if (from == to || step >= stepCount(from, to)) assert(code == 0);
        charge += code == 1 ? 1 : code == 2 ? -1 : 0;
      }
      assert(charge == potential[to] - potential[from]);
    }
  }
  // The direct dark-gray -> light-gray train did not converge upstream: it must
  // remove then apply, not silently float.
  assert(stepCount(1, 2) == 26);
  assert(stepCount(2, 1) == 7);
  for (size_t step = 0; step < 13; ++step) {
    assert(drive(3, 0, step) == 1);
    assert(drive(0, 3, step) == 2);
  }
  assert(drive(4, 0, 0) == 0);
  assert(drive(0, 4, 0) == 0);
  assert(drive(0, 3, static_cast<size_t>(-1)) == 0);
  // Exhaust all packed old/new bytes, covering mixed unchanged and changed pixels.
  for (unsigned before = 0; before < 256; ++before) {
    for (unsigned after = 0; after < 256; ++after) {
      for (size_t step = 0; step < STEPS; ++step) {
        const uint8_t packed = driveByte(before, after, step);
        for (unsigned pixel = 0; pixel < 4; ++pixel) {
          const unsigned shift = 6 - pixel * 2;
          assert(((packed >> shift) & 3) == drive((before >> shift) & 3, (after >> shift) & 3, step));
        }
      }
    }
  }
  std::puts("PaperS3 native waveform: all transitions, DC balance, fallback and packed rows passed");
}
