#pragma once
// Fake of Arduino's USBMSC with the same identity semantics: the setters copy
// into the LUN, and end() clears them (cores/esp32/USBMSC.cpp).
#include <cstdint>
#include <cstring>
#include <string>

typedef bool (*msc_start_stop_cb)(uint8_t, bool, bool);
typedef int32_t (*msc_read_cb)(uint32_t, uint32_t, void*, uint32_t);
typedef int32_t (*msc_write_cb)(uint32_t, uint32_t, uint8_t*, uint32_t);

struct FakeLun {
  std::string vendor, product, revision;
  bool media = false, writable = false;
  uint32_t blockCount = 0;
  uint16_t blockSize = 0;
  bool failBegin = false;
};
inline FakeLun gFakeLun;

class USBMSC {
 public:
  bool begin(uint32_t count, uint16_t size) {
    gFakeLun.blockCount = count;
    gFakeLun.blockSize = size;
    return !gFakeLun.failBegin && count && size;
  }
  void end() { gFakeLun = FakeLun{{}, {}, {}, false, false, 0, 0, gFakeLun.failBegin}; }
  void vendorID(const char* v) { gFakeLun.vendor = std::string(v).substr(0, 8); }
  void productID(const char* v) { gFakeLun.product = std::string(v).substr(0, 16); }
  void productRevision(const char* v) { gFakeLun.revision = std::string(v).substr(0, 4); }
  void mediaPresent(bool v) { gFakeLun.media = v; }
  void isWritable(bool v) { gFakeLun.writable = v; }
  void onStartStop(msc_start_stop_cb) {}
  void onRead(msc_read_cb) {}
  void onWrite(msc_write_cb) {}
};
