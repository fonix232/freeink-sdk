#include <BoardConfig.h>
#include <M5GFX.h>

#include <cassert>
#include <iostream>

#include "driver/LgfxEpdDriver.h"

namespace freeink {
const LgfxEpdConfig& testLgfxConfig() {
  static const LgfxEpdConfig cfg{};
  return cfg;
}
}  // namespace freeink

int main() {
  using namespace freeink;
  EpdBus bus;
  LgfxEpdDriver driver(testLgfxConfig());
  driver.begin(bus);
  assert(!lgfx::initCleared);  // Retained boot image remains untouched.
  assert(driver.grayscaleCapabilities().base == GrayscaleBase::Combined);
  const uint8_t bw[] = {0x1F, 0xFF}, lsb[] = {0x40, 0}, msb[] = {0x60, 0};
  for (auto mode : {RefreshMode::Fast, RefreshMode::Half}) {
    const auto before = lgfx::pushes;
    driver.displayGrayscaleBase(bus, bw, mode, false);
    assert(lgfx::pushes == before);
    driver.writeGrayscalePlaneStrip(bus, GrayPlane::Lsb, lsb, 0, 2);
    driver.writeGrayscalePlaneStrip(bus, GrayPlane::Msb, msb, 0, 2);
    // The caller's framebuffer may hold the last selector plane here.
    driver.displayGray(bus, msb, false, nullptr, false);
    assert(lgfx::pushes == before + 1);
    assert(lgfx::pushedMode == lgfx::epd_mode::epd_text);
    assert(lgfx::pixels[0] == 0 && lgfx::pixels[1] == 0x55 && lgfx::pixels[2] == 0xAA && lgfx::pixels[3] == 0xFF);
    driver.cleanupGrayscaleBuffers(bus, bw);
    assert(lgfx::pushes == before + 1);  // Cleanup must not overwrite gray ink.
  }
  // If AA cannot allocate its scratch, cleanup commits the staged B/W page.
  unsigned before = lgfx::pushes;
  driver.displayGrayscaleBase(bus, bw, RefreshMode::Fast, true);
  driver.cleanupGrayscaleBuffers(bus, bw);
  assert(lgfx::pushes == before + 1 && lgfx::sleeps == 1);
  assert(lgfx::pixels[1] == 0 && lgfx::pixels[2] == 0);
  // Explicit cancellation must not paint anything.
  before = lgfx::pushes;
  driver.displayGrayscaleBase(bus, bw, RefreshMode::Fast, false);
  driver.cleanupGrayscaleBuffers(bus, nullptr);
  driver.cleanupGrayscaleBuffers(bus, bw);
  assert(lgfx::pushes == before);
  // Separate-base callers still get actual grayscale on PaperS3.
  driver.display(bus, bw, nullptr, RefreshMode::Fast, false);
  driver.copyGrayscaleLsb(bus, lsb);
  driver.copyGrayscaleMsb(bus, msb);
  driver.displayGray(bus, msb, false, nullptr, false);
  assert(lgfx::pushedMode == lgfx::epd_mode::epd_text);
  // Other LGFX boards retain their original separate-base behavior.
  BoardConfig::paper = false;
  assert(driver.grayscaleCapabilities().base == GrayscaleBase::Separate);
  before = lgfx::pushes;
  driver.displayGrayscaleBase(bus, bw, RefreshMode::Fast, false);
  assert(lgfx::pushes == before + 1 && lgfx::pushedMode == lgfx::epd_mode::epd_fast);
  std::cout << "LGFX: retained boot, single grayscale update, four tones, cancellation and OOM fallback passed\n";
}
