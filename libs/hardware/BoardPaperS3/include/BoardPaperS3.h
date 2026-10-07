#pragma once

#include <Arduino.h>

#include "BoardPaperS3Pins.h"

namespace BoardPaperS3 {

// Software power-off: pulses the PWROFF line to the PMS150G latch controller.
// The PaperS3 has no firmware-held power latch — the PMS150G latches power by
// itself on the side button — so releasing power is this pulse train, not a
// latch-pin release. Does not return if the board is on battery; on USB the
// board browns back up, so treat it as best-effort there.
void powerOff();

// Retains RAM/display; timer-only nap preserves the existing touch polling cadence.
// Caller must quiesce storage/rendering and stop radios. Returns actual sleep us,
// or zero on rejection. USB is checked here as well as by the caller.
uint32_t idleSleep(uint32_t milliseconds);

// GPIO5 is an analog divider, not a digital logic-level USB detect input.
uint16_t usbMillivolts();

}  // namespace BoardPaperS3
