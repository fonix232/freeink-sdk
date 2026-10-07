#pragma once

#include "PanelDriver.h"

namespace freeink {
// Native: the default per-transition waveform (PaperS3NativeWaveform.h).
// LovyanGfx: the LovyanGFX drive tables, kept as a comparison reference.
// Epdiy: experimental EPDiy ED047TC1 tables with per-phase gate pulse timing.
enum class PaperS3Waveform : uint8_t { Native = 0, LovyanGfx = 1, Epdiy = 3 };

struct PaperS3ScanTiming {
  uint32_t totalUs = 0, convertUs = 0, waitUs = 0, submitUs = 0, rows = 0, cpuMhz = 0;
};
// ED047TC1 on PaperS3: owns the ESP32-S3 parallel bus and panel state.
// No graphics-library device or sprite participates in presentation.
class PaperS3Driver final : public PanelDriver {
 public:
  PaperS3ScanTiming lastScanTiming() const;
  // Call only between presentations, under the application render lock. The
  // next refresh after a change starts from a full clear.
  void setWaveform(PaperS3Waveform waveform);
  uint32_t spiHz() const override { return 0; }
  BusyPolarity busyPolarity() const override { return BusyPolarity::ActiveLow; }
  bool usesExternalBus() const override { return true; }
  PanelGeometry geometry() const override { return {960, 540, 120, 64800}; }
  void begin(EpdBus&) override;
  void deepSleep(EpdBus&) override;
  void display(EpdBus&, const uint8_t*, const uint8_t*, RefreshMode, bool) override;
  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode mode = GrayscaleMode::Overlay) const override;
  void displayGrayscaleBase(EpdBus&, const uint8_t*, RefreshMode, bool) override;
  void copyGrayscaleLsb(EpdBus&, const uint8_t*) override;
  void copyGrayscaleMsb(EpdBus&, const uint8_t*) override;
  void writeGrayscalePlaneStrip(EpdBus&, GrayPlane, const uint8_t*, uint16_t, uint16_t) override;
  void displayGray(EpdBus&, const uint8_t*, bool, const unsigned char*, bool) override;
  void cleanupGrayscaleBuffers(EpdBus&, const uint8_t*) override;
};
PaperS3Driver& paperS3Driver();
}  // namespace freeink
