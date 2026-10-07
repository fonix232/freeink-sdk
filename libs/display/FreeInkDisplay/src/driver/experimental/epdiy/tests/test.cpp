#if defined(FREEINK_PAPERS3_EPDIY_TEST)
#include "../PaperS3EpdiyWaveforms.h"

#include <cassert>
#include <cstdint>
#include <initializer_list>

using namespace freeink::papers3::epdiy;

struct FakeTransport {
  bool timingSupported = true;
  unsigned calls = 0;
  unsigned failAt = 999;
  bool aborted = false;
  bool supportsTiming(uint16_t high, uint16_t low) { return timingSupported && high >= 8 && low == 50; }
  bool scan(Phase) { return calls++ != failAt; }
  void abort() { aborted = true; }
};

int main() {
  static_assert(phaseCount(Mode::Text) == 31);
  static_assert(phaseCount(Mode::Grayscale) == 31);
  static_assert(phaseCount(Mode::DirectMono) == 6);
  static_assert(drive(3, 0, DIRECT_MONO[0]) == 1);
  static_assert(drive(0, 3, DIRECT_MONO[0]) == 2);
  static_assert(drive(0, 0, DIRECT_MONO[0]) == 0);
  static_assert(drive(3, 3, TEXT[0]) == 0);
  static_assert(drive(3, 3, GRAYSCALE[0]) == 1);
  static_assert(drive(4, 0, TEXT[0]) == 0);
  for (auto mode : {Mode::DirectMono, Mode::Grayscale, Mode::Text}) {
    for (size_t step = 0; step < phaseCount(mode); ++step) {
      const auto pulse = phase(mode, step);
      assert(pulse.gateHighTenthsUs >= 8 && pulse.gateHighTenthsUs <= 1000);
      for (unsigned before = 0; before < 256; ++before) {
        for (unsigned after = 0; after < 256; ++after) {
          const auto packed = driveByte(before, after, pulse);
          for (unsigned shift = 0; shift < 8; shift += 2) {
            const auto expected = drive((before >> shift) & 3, (after >> shift) & 3, pulse);
            assert(expected <= 2);
            assert(((packed >> shift) & 3) == expected);
          }
        }
      }
    }
    assert(phase(mode, phaseCount(mode) - 1).packedTransitions == 0);
    assert(phase(mode, 999).packedTransitions == 0);
    FakeTransport ok;
    assert(run(mode, ok));
    assert(ok.calls == phaseCount(mode) && !ok.aborted);
    FakeTransport unsupported;
    unsupported.timingSupported = false;
    assert(!run(mode, unsupported));
    assert(unsupported.calls == 0 && !unsupported.aborted);
    FakeTransport failure;
    failure.failAt = 2;
    assert(!run(mode, failure));
    assert(failure.calls == 3 && failure.aborted);
  }
}

#endif
