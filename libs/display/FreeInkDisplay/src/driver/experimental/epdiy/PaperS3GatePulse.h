#pragma once

#include <cstdint>

namespace freeink::papers3::epdiy {
// Owns RMT group 0 exclusively while acquired. No RMT library/Arduino users may
// coexist. Acquire after frame-start GPIO pulses; release before frame-end ones.
class GatePulse {
 public:
  GatePulse() = default;
  GatePulse(const GatePulse&) = delete;
  GatePulse& operator=(const GatePulse&) = delete;
  bool acquire(int gpio);
  bool start(uint16_t highTenthsUs, uint16_t lowTenthsUs);
  bool wait(uint32_t timeoutUs = 1000);
  void release();
  bool active() const { return acquired; }

 private:
  int pin = -1;
  bool acquired = false;
  bool pending = false;
};
}  // namespace freeink::papers3::epdiy
