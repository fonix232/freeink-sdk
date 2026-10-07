#include <BoardPaperS3.h>

namespace BoardPaperS3 {

uint16_t usbMillivolts() {
  static bool sampled = false;
  static unsigned long sampledAt = 0;
  static uint16_t millivolts = 0;
  const auto now = millis();
  if (!sampled || now - sampledAt >= 100) {
    millivolts = analogReadMilliVolts(5);
    sampledAt = now;
    sampled = true;
  }
  return millivolts;
}

void powerOff() {
  // A single edge is not enough for the PMS150G — M5Unified's power-off path
  // notes "the power cannot be turned off simply by setting the GPIO to LOW"
  // and drives a 5x 50 ms low/high pulse train; mirror it.
  pinMode(PAPERS3_PWROFF_PULSE, OUTPUT);
  for (int i = 0; i < 5; ++i) {
    digitalWrite(PAPERS3_PWROFF_PULSE, LOW);
    delay(50);
    digitalWrite(PAPERS3_PWROFF_PULSE, HIGH);
    delay(50);
  }
  digitalWrite(PAPERS3_PWROFF_PULSE, LOW);
}

}  // namespace BoardPaperS3
