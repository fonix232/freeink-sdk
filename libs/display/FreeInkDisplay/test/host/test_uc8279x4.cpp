#include <BoardConfig.h>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <vector>

#include "driver/Uc8279X4Driver.h"

#if FREEINK_UC8279X4_COMBINED_TEXT
static void testCombined() {
  using namespace freeink;
  constexpr size_t n = 800 / 8 * 480;
  std::vector<uint8_t> bw(n, 0xFF), lsb(n, 0), msb(n, 0);
  bw[0] = 0x1F;
  lsb[0] = 0x40;
  msb[0] = 0x60;
  constexpr unsigned pairs = 6;
  for (unsigned variant : {2u, 0x68u, 0x69u}) {
    BoardConfig::ACTIVE.displayControllerVariant = variant;
    EpdBus bus;
    Uc8279X4Driver driver;
    driver.begin(bus);
    driver.setReaderRendering(true);
    assert(driver.grayscaleCapabilities().base == GrayscaleBase::Combined);
    std::vector<std::vector<uint8_t>> quality;
    for (int page = 0; page < 4; ++page) {
      const unsigned before = bus.refreshes;
      const auto mode = page == 3 ? RefreshMode::Half : RefreshMode::Fast;
      driver.displayGrayscaleBase(bus, bw.data(), mode, false);
      driver.copyGrayscaleLsb(bus, lsb.data());
      driver.copyGrayscaleMsb(bus, msb.data());
      assert(bus.refreshes == before);  // No intermediate B/W page.
      assert(bus.oldPlane[12000] == 0xA0 && bus.newPlane[12000] == 0xC0);
      driver.displayGray(bus, msb.data(), false, nullptr, false);
      assert(bus.refreshes == before + 1);
      assert(bus.rawRegisters == std::vector<uint8_t>({0x20, 0x21, 0x23, 0x22, 0x24}));
      if (page == 0) quality = bus.banks;
      if (page == 0 || page == 3) {
        assert(bus.banks == quality);  // Initial and scheduled cleanup.
      } else {
        for (unsigned table = 0; table < 5; ++table) {
          unsigned removed[3] = {};
          for (unsigned group = 0; group < 7; ++group) {
            for (unsigned byte = 0; byte < 7; ++byte) {
              const unsigned i = group * 7 + byte;
              auto original = quality[table][i];
              if (byte >= 1 && byte <= 4) {
                if ((table == 1 || table == 4) && (original >> 6) != (table == 1 ? 1u : 2u)) original &= 0x3F;
                const auto tuned = bus.banks[table][i];
                assert((tuned >> 6) == (original >> 6));
                assert((tuned & 0x3F) <= (original & 0x3F));
                if (original & 0x3F) assert(tuned & 0x3F);
                removed[original >> 6] += (original & 0x3F) - (tuned & 0x3F);
              } else {
                assert(bus.banks[table][i] == original);
              }
            }
          }
          const bool gray = table == 2 || table == 3;
          assert(removed[0] == (gray ? 0u : 2u * pairs));
          assert(removed[1] == (gray ? pairs : 0u) && removed[2] == (gray ? pairs : 0u));
        }
        for (unsigned table : {1u, 4u}) {
          bool driven = false;
          for (unsigned group = 0; group < 7; ++group) {
            for (unsigned byte = 0; byte < 7; ++byte) {
              const unsigned i = group * 7 + byte;
              const auto pulse = bus.banks[table][i];
              if (byte >= 1 && byte <= 4) {
                if (pulse >> 6) assert((pulse & 0x3F) == (quality[table][i] & 0x3F));
                const unsigned rail = pulse >> 6;
                assert(rail == 0 || rail == (table == 1 ? 1u : 2u));
                driven |= rail != 0 && (pulse & 0x3F) != 0;
              } else {
                assert(pulse == quality[table][i]);  // Repeat/timing controls unchanged.
              }
            }
          }
          assert(driven);  // Neither solid target is accidentally held.
        }
      }
      driver.cleanupGrayscaleBuffers(bus, bw.data());
      assert(bus.refreshes == before + 1);
    }
    // An incomplete AA pass cannot activate stale/missing planes.
    unsigned before = bus.refreshes;
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, msb.data(), false, nullptr, false);
    assert(bus.refreshes == before);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.displayGray(bus, lsb.data(), false, nullptr, false);
    assert(bus.refreshes == before);
    driver.cleanupGrayscaleBuffers(bus, bw.data());  // Allocation/error fallback paints B/W.
    assert(bus.refreshes == before + 1);
    // Explicit cancel does not display the abandoned page.
    before = bus.refreshes;
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
    driver.cleanupGrayscaleBuffers(bus, nullptr);
    assert(bus.refreshes == before);
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, msb.data(), false, nullptr, false);
    assert(bus.banks == quality);  // Cancellation requests a physical rebase.
    // Absolute images stage both planes; factory requests retain the quality bank.
    before = bus.refreshes;
    driver.beginGrayscale(bus, bw.data(), GrayscaleMode::Absolute, RefreshMode::Half, false);
    assert(bus.refreshes == before);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, msb.data(), false, nullptr, true);
    assert(bus.refreshes == before + 1 && bus.banks == quality);
  }
  std::cout << "UC8279 combined text: one "
               "activation, balanced frame trims, "
               "unchanged solid drive, cleanup, "
               "incomplete planes, cancellation and images passed\n";
}
static void testReaderWaveformWithImages() {
  using namespace freeink;
  constexpr size_t n = 800 / 8 * 480;
  std::vector<uint8_t> bw(n, 0x33), lsb(n, 0x55), msb(n, 0xAA);
  for (unsigned variant : {2u, 0x68u, 0x69u}) {
    BoardConfig::ACTIVE.displayControllerVariant = variant;
    EpdBus bus;
    Uc8279X4Driver driver;
    driver.begin(bus);
    driver.setReaderRendering(true);
    // Default G, with an initial cleanup before normal refreshes.
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, bw.data(), false, nullptr, false);
    const auto quality = bus.banks;
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, bw.data(), false, nullptr, false);
    const auto selected = bus.banks;
    assert(selected != quality);  // Dense images use the selected bank too.
    // B/W reader pages: Fast uses the selected text bank, Half (scheduled
    // cleanup) the quality bank, matching grayscale pages. Repeat to check the
    // cleanup does not leave the next Fast page on the quality bank.
    for (const auto mode : {RefreshMode::Fast, RefreshMode::Half, RefreshMode::Fast, RefreshMode::Half}) {
      const auto& expected = mode == RefreshMode::Half ? quality : selected;
      auto before = bus.refreshes;
      driver.display(bus, bw.data(), nullptr, mode, true);
      assert(bus.refreshes == before + 1 && bus.banks == expected);
      assert(bus.oldPlane[12000] == bw[0] && bus.newPlane[12000] == bw[0]);
      before = bus.refreshes;
      assert(!driver.displayStart(bus, bw.data(), nullptr, mode, false));
      driver.displayFinish(bus, bw.data());
      assert(bus.refreshes == before + 1 && bus.banks == expected);
    }
    for (const auto mode : {GrayscaleMode::Absolute, GrayscaleMode::Direct}) {
      const auto before = bus.refreshes;
      assert(driver.grayscaleCapabilities(mode).base == GrayscaleBase::Combined);
      driver.beginGrayscale(bus, bw.data(), mode, RefreshMode::Fast, false);
      driver.copyGrayscaleLsb(bus, lsb.data());
      driver.copyGrayscaleMsb(bus, msb.data());
      assert(bus.refreshes == before);
      assert(bus.oldPlane[12000] == uint8_t(~lsb[0]));
      assert(bus.newPlane[12000] == uint8_t(~msb[0]));
      driver.displayGray(bus, bw.data(), false, nullptr, false);
      assert(bus.refreshes == before + 1 && bus.banks == selected);
      assert(bus.oldPlane[12000] == uint8_t(lsb[0] & msb[0]));
      assert(bus.oldPlane == bus.newPlane);
    }
    bus.psr.clear();
    driver.display(bus, bw.data(), nullptr, RefreshMode::Full, false);
    assert(!(bus.psr[0] & 0x20));  // Explicit full refresh retains OTP cleanup.
    driver.requestResync(1);
    bus.psr.clear();
    driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
    assert(!(bus.psr[0] & 0x20));
  }
  std::cout << "Reader waveform: B/W pages, dense images, absolute/direct planes and full cleanup passed\n";
}

static void testSystemRendering() {
  using namespace freeink;
  constexpr size_t n = 800 / 8 * 480;
  std::vector<uint8_t> bw(n, 0x33), lsb(n, 0x55), msb(n, 0xAA);
  for (unsigned variant : {2u, 0x68u, 0x69u}) {
    BoardConfig::ACTIVE.displayControllerVariant = variant;
    EpdBus bus;
    Uc8279X4Driver driver;
    driver.begin(bus);
    assert(driver.supportsAsyncDisplay());
    assert(driver.grayscaleCapabilities(GrayscaleMode::Absolute).base == GrayscaleBase::Separate);
    for (const auto mode : {RefreshMode::Fast, RefreshMode::Fast, RefreshMode::Half}) {
      bus.psr.clear();
      bus.banks.clear();
      const auto before = bus.refreshes;
      driver.display(bus, bw.data(), nullptr, mode, false);
      assert(bus.refreshes == before + 1 && bus.banks.empty());
      assert(!bus.psr.empty() && !(bus.psr[0] & 0x20));
    }
    driver.setReaderRendering(true);
    std::vector<std::vector<uint8_t>> quality, g;
    for (unsigned page = 0; page < 2; ++page) {
      driver.beginGrayscale(bus, bw.data(), GrayscaleMode::Absolute, RefreshMode::Fast, false);
      driver.copyGrayscaleLsb(bus, lsb.data());
      driver.copyGrayscaleMsb(bus, msb.data());
      driver.displayGray(bus, bw.data(), false, nullptr, false);
      if (!page)
        quality = bus.banks;
      else
        g = bus.banks;
    }
    assert(g != quality && !driver.supportsAsyncDisplay());
    driver.setReaderRendering(false);
    assert(driver.supportsAsyncDisplay());
    bus.psr.clear();
    bus.banks.clear();
    driver.display(bus, bw.data(), nullptr, RefreshMode::Fast, false);
    assert(bus.banks.empty() && !(bus.psr[0] & 0x20));
    // Explicit system images (such as sleep covers) retain the quality bank,
    // even after leaving a reader that used G.
    driver.beginGrayscale(bus, bw.data(), GrayscaleMode::Direct, RefreshMode::Fast, false);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, bw.data(), false, nullptr, false);
    assert(bus.banks == quality);
  }
  std::cout << "System rendering: OTP B/W, async support, reader transitions and quality-only system images passed\n";
}

#endif

int main() {
#if FREEINK_UC8279X4_COMBINED_TEXT
  testCombined();
  testReaderWaveformWithImages();
  testSystemRendering();
  return 0;
#endif
  using namespace freeink;
  constexpr size_t n = 800 / 8 * 480;
  // Sparse AA includes both edge shades plus solid black and white.
  std::vector<uint8_t> bw(n, 0xFF), lsb(n, 0), msb(n, 0);
  bw[0] = 0x1F;
  lsb[0] = 0x40;
  msb[0] = 0x60;

  for (unsigned variant : {2u, 0x68u, 0x69u}) {
    std::fill(msb.begin(), msb.end(), 0);
    msb[0] = 0x60;
    BoardConfig::ACTIVE.displayControllerVariant = variant;
    EpdBus bus;
    Uc8279X4Driver driver;
    driver.begin(bus);
    driver.setReaderRendering(true);
    std::vector<std::vector<uint8_t>> textBank;
    for (int page = 0; page < 3; ++page) {
      bus.psr.clear();
      const unsigned before = bus.refreshes;
      driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Fast, false);
      if (page > 0) {
        // Subsequent text pages use the external short transition, not OTP GC.
        assert(bus.psr.size() == 2 && (bus.psr[0] & 0x20));
      }
      driver.copyGrayscaleLsb(bus, lsb.data());
      driver.copyGrayscaleMsb(bus, msb.data());
      // Inverted absolute selectors preserve dark=01 and light=10 on the wire.
      assert(bus.oldPlane[12000] == 0xA0);
      assert(bus.newPlane[12000] == 0xC0);
      driver.displayGray(bus, bw.data(), false, nullptr, false);
      assert(bus.refreshes == before + 2);  // base transition + edge-only AA
      assert(bus.rawRegisters == std::vector<uint8_t>({0x20, 0x21, 0x23, 0x22, 0x24}));
      textBank = bus.banks;
      assert(textBank[2] != textBank[3]);
      // Solid pixels receive exactly the neutral VCOM waveform, hence no
      // source-to-common drive; gray pixels retain the quality bank below.
      assert(textBank[1] == textBank[0] && textBank[4] == textBank[0]);
      for (unsigned group = 0; group < 7; ++group)
        for (unsigned phase = 1; phase <= 4; ++phase) assert((textBank[0][group * 7 + phase] & 0xC0) == 0);
      // Restoring RAM does not physically refresh the panel.
      const auto refreshed = bus.refreshes;
      driver.cleanupGrayscaleBuffers(bus, bw.data());
      assert(bus.refreshes == refreshed);
      assert(bus.oldPlane[12000] == bw[0] && bus.newPlane[12000] == bw[0]);
    }
    // Explicit periodic/manual cleanup still requests the OTP B/W waveform.
    bus.psr.clear();
    driver.displayGrayscaleBase(bus, bw.data(), RefreshMode::Half, false);
    assert(bus.psr.size() == 2 && !(bus.psr[0] & 0x20));
    // Large images retain the distinct quality bank.
    std::fill(msb.begin(), msb.end(), 0xFF);
    driver.copyGrayscaleLsb(bus, lsb.data());
    driver.copyGrayscaleMsb(bus, msb.data());
    driver.displayGray(bus, bw.data(), false, nullptr, false);
    assert(bus.rawRegisters == std::vector<uint8_t>({0x20, 0x21, 0x23, 0x22, 0x24}));
    assert(bus.banks[0] == textBank[0]);
    assert(bus.banks[2] == textBank[2] && bus.banks[3] == textBank[3]);
    assert(bus.banks[1] != textBank[1] && bus.banks[4] != textBank[4]);
  }
  std::cout << "UC8279 X4 edge AA: variants, gray selectors, neutral solid pixels, unchanged quality pulses, "
               "transitions and images passed\n";
}
