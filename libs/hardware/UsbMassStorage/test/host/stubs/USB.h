#pragma once
// Fake of Arduino's ESPUSB: begin() result is test-controlled.
struct FakeUsb {
  bool fail = false;
  unsigned begins = 0;
  bool begin() {
    ++begins;
    return !fail;
  }
};
inline FakeUsb USB;
