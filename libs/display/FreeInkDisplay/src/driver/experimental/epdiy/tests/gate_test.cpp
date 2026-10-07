#if defined(FREEINK_PAPERS3_EPDIY_TEST)
#include "stubs/Mock.h"
#include "../PaperS3GatePulse.cpp"
#include <cassert>

PaperS3RmtMemory RMTMEM{};

int main() {
  using freeink::papers3::epdiy::GatePulse;
  GatePulse pulse, other;
  assert(!pulse.start(100, 50));
  assert(!pulse.acquire(-1));
  assert(pulse.acquire(18));
  assert(peripheralEnabled && routedSignal == 42);
  assert(!other.acquire(18));
  assert(!pulse.start(0, 50));
  assert(!pulse.start(100, 0));
  assert(!pulse.start(32768, 50));
  assert(pulse.start(8, 50));
  assert(RMTMEM.channel[0].words[0] == (8u | 0x8000u | (50u << 16)));
  assert(RMTMEM.channel[0].words[1] == 0);
  assert(!pulse.start(8, 50));
  assert(pulse.wait());
  pulse.release();
  assert(!peripheralEnabled && routedSignal == SIG_GPIO_OUT_IDX);
  assert(other.acquire(18));
  autoComplete = false;
  assert(other.start(100, 50));
  assert(!other.wait(10));
  assert(!other.active() && !peripheralEnabled && routedSignal == SIG_GPIO_OUT_IDX);
  assert(pulse.acquire(18));
  assert(pulse.start(100, 50));
  rawStatus = 2;
  assert(!pulse.wait());
  assert(!pulse.active() && !peripheralEnabled);
}

#endif
