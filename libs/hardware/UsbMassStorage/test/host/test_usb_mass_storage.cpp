#include <cassert>
#include <cstdio>

#include <USB.h>
#include <USBMSC.h>

#include "UsbMassStorage.h"

extern "C" bool tud_mounted(void) { return false; }
extern "C" bool tud_suspended(void) { return false; }
extern "C" bool tud_disconnect(void) { return true; }

namespace {
struct Card : FsBlockDeviceInterface {
  Sector_t sectorCount() override { return 1024; }
};

void assertIdentity() {
  assert(gFakeLun.vendor == "FreeInk");
  assert(gFakeLun.product == "SD Card");
  assert(gFakeLun.revision == "1.0");
}
}  // namespace

int main() {
  using freeink::UsbMassStorage;
  Card card;
  UsbMassStorage msc;

  // Registered at static initialization, before composite USB starts.
  assertIdentity();
  assert(!gFakeLun.media);

  // start -> stop -> start: end() clears the strings; begin() restores them.
  for (int cycle = 0; cycle < 2; ++cycle) {
    assert(msc.begin(&card));
    assertIdentity();
    assert(gFakeLun.media && gFakeLun.blockCount == 1024);
    msc.end();
    assert(gFakeLun.vendor.empty() && !gFakeLun.media);
  }

  // A failed USB start tears down the LUN; the retry must restore identity.
  USB.fail = true;
  assert(!msc.begin(&card));
  assert(gFakeLun.vendor.empty() && !msc.active());
  USB.fail = false;
  assert(msc.begin(&card));
  assertIdentity();
  msc.end();

  // Same for a rejected geometry.
  gFakeLun.failBegin = true;
  assert(!msc.begin(&card));
  gFakeLun.failBegin = false;
  assert(msc.begin(&card));
  assertIdentity();
  msc.end();

  std::puts("usb mass storage: identity survives start/stop and failed-start retries");
  return 0;
}
