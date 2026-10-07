#include <BoardConfig.h>

#if FREEINK_DRIVER_PAPERS3
#include <BoardPaperS3Pins.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_private/gdma.h>
#include <esp_private/periph_ctrl.h>
#include <esp_rom_gpio.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/dma_types.h>
#include <hal/gpio_ll.h>
#include <hal/lcd_ll.h>
#include <soc/gpio_sig_map.h>
#include <soc/gpio_struct.h>
#include <soc/lcd_cam_struct.h>

#include <cstring>

#include "PaperS3Driver.h"
#include "PaperS3LovyanWaveforms.h"
#include "PaperS3NativeWaveform.h"

// Boot-time waveform: 0 native, 1 LovyanGFX reference. setWaveform() overrides it.
#ifndef FREEINK_PAPERS3_WAVEFORM
#define FREEINK_PAPERS3_WAVEFORM 0
#endif
static_assert(FREEINK_PAPERS3_WAVEFORM == 0 || FREEINK_PAPERS3_WAVEFORM == 1, "Invalid PaperS3 waveform");

// Enable only while diagnosing scan stages; per-row timers perturb the waveform.
#ifndef FREEINK_PAPERS3_PROFILE_SCANS
#define FREEINK_PAPERS3_PROFILE_SCANS 0
#endif

namespace freeink {
namespace {
constexpr size_t WIDTH = 960, HEIGHT = 540, ROW_BYTES = WIDTH / 4;
constexpr size_t FRAME_BYTES = ROW_BYTES * HEIGHT, PLANE_BYTES = WIDTH * HEIGHT / 8;
// Each row is followed by eight padding bytes, as in the vendor driver.
constexpr size_t DMA_BYTES = ROW_BYTES + 8;
// Pause after each scan: the native trains are calibrated to EPD_Painter's
// 4 ms post-scan wait, the LovyanGFX tables to 250 us.
constexpr uint32_t LOVYAN_SCAN_GAP_US = 250;
PaperS3Waveform selectedWaveform = static_cast<PaperS3Waveform>(FREEINK_PAPERS3_WAVEFORM);
constexpr char TAG[] = "PaperS3";

struct State {
  gdma_channel_handle_t dma = nullptr;
  dma_descriptor_t descriptors[2]{};
  bool peripheral = false, connected = false;
  uint8_t* memory = nullptr;
  uint8_t* previous = nullptr;
  uint8_t* target = nullptr;
  uint8_t* lsb = nullptr;
  uint8_t* msb = nullptr;
  uint16_t* transitions = nullptr;
  uint8_t* row = nullptr;
  uint8_t driveLut[256]{};
  uint8_t unchangedLut[256]{};
  uint8_t transitionLut[256]{};
  bool ready = false, powered = false, baseline = false, pending = false, turnOff = false;
  RefreshMode mode = RefreshMode::Full;
  PaperS3ScanTiming timing;
} state;

void pin(int pin, bool high) { gpio_set_level(static_cast<gpio_num_t>(pin), high); }

void power(bool on) {
  if (state.powered == on) return;
  if (on) {
    pin(PAPERS3_EP_OE, true);
    delayMicroseconds(100);
    pin(PAPERS3_EP_PWR, true);
    delayMicroseconds(100);
    pin(PAPERS3_EP_SPV, true);
    delay(1);
  } else {
    delay(1);
    pin(PAPERS3_EP_PWR, false);
    delayMicroseconds(10);
    pin(PAPERS3_EP_OE, false);
    delayMicroseconds(100);
    pin(PAPERS3_EP_SPV, false);
  }
  state.powered = on;
}

void release() {
  state.powered = true;  // Also park rails after partial initialization.
  power(false);
  if (state.peripheral) lcd_ll_stop(&LCD_CAM);
  if (state.dma) {
    gdma_stop(state.dma);
    if (state.connected) gdma_disconnect(state.dma);
    gdma_del_channel(state.dma);
  }
  if (state.peripheral) periph_module_disable(PERIPH_LCD_CAM_MODULE);
  heap_caps_free(state.row);
  heap_caps_free(state.memory);
  state = {};
}

void startFrame() {
  pin(PAPERS3_EP_SPV, false);
  delayMicroseconds(1);
  pin(PAPERS3_EP_CKV, false);
  delayMicroseconds(3);
  pin(PAPERS3_EP_CKV, true);
  delayMicroseconds(1);
  pin(PAPERS3_EP_SPV, true);
  for (unsigned i = 0; i < 3; ++i) {
    delayMicroseconds(3);
    pin(PAPERS3_EP_CKV, false);
    delayMicroseconds(3);
    pin(PAPERS3_EP_CKV, true);
  }
}

bool waitLine() {
  const auto started = esp_timer_get_time();
  while (LCD_CAM.lcd_user.lcd_start) {
    if (esp_timer_get_time() - started < 100000) continue;
    // Do not reuse the buffers after a stalled peripheral.
    state.ready = false;
    state.baseline = false;
    lcd_ll_stop(&LCD_CAM);
    gdma_stop(state.dma);
    power(false);
    ESP_LOGE(TAG, "scanline transfer timed out");
    return false;
  }
  pin(PAPERS3_EP_SPH, true);
  pin(PAPERS3_EP_CKV, false);
  pin(PAPERS3_EP_LE, true);
  return true;
}

bool submitLine(size_t index) {
  auto& descriptor = state.descriptors[index];
  descriptor.dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
  lcd_ll_fifo_reset(&LCD_CAM);
  if (gdma_start(state.dma, reinterpret_cast<intptr_t>(&descriptor)) != ESP_OK) return false;
  // Prime the LCD FIFO before starting its clock. Keep IDF's 4 us margin
  // initially; unlike the general i80 API this needs no transaction queue.
  delayMicroseconds(4);
  pin(PAPERS3_EP_LE, false);
  pin(PAPERS3_EP_CKV, true);
  pin(PAPERS3_EP_SPH, false);
  lcd_ll_start(&LCD_CAM);
  return true;
}

#if defined(__XTENSA__)
__attribute__((optimize("O3")))
#endif
void convertTransitions(uint8_t* __restrict output, const uint16_t* __restrict input, const uint8_t* __restrict lut) {
  for (size_t x = 0; x < ROW_BYTES; ++x) {
    const uint16_t pair = input[x];
    output[x] = (lut[pair & 255] << 4) | lut[pair >> 8];
  }
}

uint8_t transitionDrive(uint8_t before, uint8_t after, size_t step) {
  return selectedWaveform == PaperS3Waveform::Native ? papers3::native::drive(before, after, step)
                                                     : papers3::lovyan::differentialDrive(before, after, step);
}

// One full panel scan. fillRow writes the drive codes for a row into its DMA
// buffer while the previous row is still transmitting.
template <typename FillRow>
bool scan(FillRow fillRow) {
  startFrame();
  for (size_t y = 0; y < HEIGHT; ++y) {
#if FREEINK_PAPERS3_PROFILE_SCANS
    const auto convertStart = esp_timer_get_time();
#endif
    auto* row = state.row + (y & 1) * DMA_BYTES;
    fillRow(row, y * ROW_BYTES);
#if FREEINK_PAPERS3_PROFILE_SCANS
    state.timing.convertUs += esp_timer_get_time() - convertStart;
    const auto waitStart = esp_timer_get_time();
#endif
    if (y && !waitLine()) return false;
#if FREEINK_PAPERS3_PROFILE_SCANS
    state.timing.waitUs += esp_timer_get_time() - waitStart;
    const auto submitStart = esp_timer_get_time();
#endif
    if (!submitLine(y & 1)) {
      state.ready = false;
      state.baseline = false;
      power(false);
      ESP_LOGE(TAG, "scanline submission failed");
      return false;
    }
#if FREEINK_PAPERS3_PROFILE_SCANS
    state.timing.submitUs += esp_timer_get_time() - submitStart;
#endif
    ++state.timing.rows;
  }
  if (!waitLine()) return false;
  delayMicroseconds(selectedWaveform == PaperS3Waveform::Native ? papers3::native::POST_SCAN_GAP_US
                                                                : LOVYAN_SCAN_GAP_US);
  pin(PAPERS3_EP_LE, false);
  pin(PAPERS3_EP_CKV, true);
  return true;
}

// Every pixel receives the same code: 0 float, 0x55 darken, 0xaa whiten.
bool uniformScan(uint8_t code) {
  return scan([code](uint8_t* row, size_t) { memset(row, code, ROW_BYTES); });
}

// Step `step` of the selected waveform for every old/new tone pair.
bool transitionScan(size_t step) {
  for (unsigned value = 0; value < 256; ++value) {
    // Two old and two new tones index four output drive bits. Keeping this
    // table in internal RAM avoids per-pixel waveform dispatch in the DMA loop.
    state.transitionLut[value] =
        (transitionDrive(value >> 6, (value >> 2) & 3, step) << 2) | transitionDrive((value >> 4) & 3, value & 3, step);
  }
  return scan(
      [](uint8_t* row, size_t offset) { convertTransitions(row, state.transitions + offset, state.transitionLut); });
}

// One LovyanGFX phase applied to the old (erase) or new tones; unchanged
// pixels are left undriven unless force is set.
bool lovyanPhaseScan(uint32_t phase, bool erase, bool force) {
  for (unsigned value = 0; value < 256; ++value) {
    state.driveLut[value] = papers3::lovyan::driveByte(value, value, phase, false, true);
  }
  return scan([erase, force](uint8_t* row, size_t offset) {
    for (size_t x = 0; x < ROW_BYTES; ++x) {
      const auto before = state.previous[offset + x];
      const auto after = state.target[offset + x];
      row[x] = state.driveLut[erase ? before : after] | (force ? 0 : state.unchangedLut[before ^ after]);
    }
  });
}

bool lovyanRun(const uint32_t* table, size_t count, bool erase, bool force) {
  for (size_t phase = 0; phase < count; ++phase) {
    // The terminal zero is a neutral scan which releases the source outputs.
    if (!lovyanPhaseScan(table[phase], erase, force || table[phase] == 0)) return false;
    if (!table[phase]) break;
  }
  return true;
}

// Packs the old/new tone pairs of every byte for transitionScan().
void prepareTransitions(bool fromWhite) {
  for (size_t i = 0; i < FRAME_BYTES; ++i) {
    const uint8_t before = fromWhite ? 0xff : state.previous[i];
    const auto after = state.target[i];
    state.transitions[i] = ((before & 0xf0) | (after >> 4)) | ((((before & 0x0f) << 4) | (after & 0x0f)) << 8);
  }
}

bool commitNative(bool force) {
  bool ok = true;
  if (force) {
    // Unknown glass: drive every pixel fully black, then fully white, and plan
    // the target from that known white.
    for (size_t i = 0; i < papers3::native::BASE_STEPS && ok; ++i) ok = uniformScan(0x55);
    for (size_t i = 0; i < papers3::native::BASE_STEPS && ok; ++i) ok = uniformScan(0xaa);
    if (ok) ok = uniformScan(0);
  }
  for (size_t step = 0; step < papers3::native::STEPS && ok; ++step) ok = transitionScan(step);
  return ok;
}

bool commitLovyan(bool grayscale, bool force) {
  using namespace papers3::lovyan;
  if (grayscale && !force) {
    bool ok = true;
    for (size_t step = 0; step < DIFFERENTIAL_STEPS && ok; ++step) ok = transitionScan(step);
    return ok;
  }
  if (force) {
    return lovyanRun(ERASE, sizeof(ERASE) / sizeof(ERASE[0]), true, true) &&
           lovyanRun(TEXT, sizeof(TEXT) / sizeof(TEXT[0]), false, true);
  }
  return lovyanRun(FAST, sizeof(FAST) / sizeof(FAST[0]), false, false);
}

void commit(bool grayscale, bool turnOff) {
  if (!state.ready) return;
  const bool force = !state.baseline || state.mode != RefreshMode::Fast;
  if (!force && memcmp(state.previous, state.target, FRAME_BYTES) == 0) {
    state.pending = false;
    return;
  }
  const auto started = esp_timer_get_time();
  state.timing = {};
  state.timing.cpuMhz = getCpuFrequencyMhz();
  const bool native = selectedWaveform == PaperS3Waveform::Native;
  if (native || (grayscale && !force)) {
    prepareTransitions(native && force);
#if FREEINK_PAPERS3_PROFILE_SCANS
    state.timing.convertUs = esp_timer_get_time() - started;
#endif
  }
  power(true);
  const bool ok = native ? commitNative(force) : commitLovyan(grayscale, force);
  if (ok) {
    memcpy(state.previous, state.target, FRAME_BYTES);
    state.baseline = true;
  }
  state.pending = false;
  // Raw panels need no powered rails between refreshes.
  (void)turnOff;
  power(false);
  state.timing.totalUs = esp_timer_get_time() - started;
  ESP_LOGI(TAG, "refresh waveform=%u gray=%d force=%d ok=%d us=%lld", static_cast<unsigned>(selectedWaveform),
           grayscale, force, ok, static_cast<long long>(esp_timer_get_time() - started));
}

void loadBw(const uint8_t* fb) {
  for (size_t i = 0; i < FRAME_BYTES; ++i) {
    const uint8_t nibble = (fb[i / 2] >> ((i & 1) ? 0 : 4)) & 15;
    uint8_t value = 0;
    for (unsigned p = 0; p < 4; ++p)
      if (nibble & (8 >> p)) value |= 3 << (6 - 2 * p);
    state.target[i] = value;
  }
}
}  // namespace

void PaperS3Driver::setWaveform(PaperS3Waveform waveform) {
  if (waveform != PaperS3Waveform::LovyanGfx) waveform = PaperS3Waveform::Native;
  if (selectedWaveform == waveform) return;
  // Caller serializes with presentation; all row transfers are synchronous.
  // The other waveform cannot start from this one's optical state.
  power(false);
  selectedWaveform = waveform;
  state.baseline = false;
  state.pending = false;
  ESP_LOGI(TAG, "waveform=%u", static_cast<unsigned>(waveform));
}

PaperS3ScanTiming PaperS3Driver::lastScanTiming() const { return state.timing; }

void PaperS3Driver::begin(EpdBus&) {
  if (state.ready || state.memory) return;
  constexpr int pins[] = {PAPERS3_EP_D0,  PAPERS3_EP_D1,  PAPERS3_EP_D2,  PAPERS3_EP_D3,
                          PAPERS3_EP_D4,  PAPERS3_EP_D5,  PAPERS3_EP_D6,  PAPERS3_EP_D7,
                          PAPERS3_EP_SPH, PAPERS3_EP_SPV, PAPERS3_EP_OE,  PAPERS3_EP_LE,
                          PAPERS3_EP_CL,  PAPERS3_EP_CKV, PAPERS3_EP_PWR, PAPERS3_PWROFF_PULSE};
  for (int p : pins) {
    gpio_reset_pin(static_cast<gpio_num_t>(p));
    gpio_set_direction(static_cast<gpio_num_t>(p), GPIO_MODE_OUTPUT);
    pin(p, false);
  }
  // Images, selector planes and reusable transition indices live in PSRAM.
  // The only internal pixel allocation is two alternating DMA scanlines.
  state.memory =
      static_cast<uint8_t*>(heap_caps_malloc(4 * FRAME_BYTES + 2 * PLANE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  state.row = static_cast<uint8_t*>(heap_caps_malloc(2 * DMA_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (!state.memory || !state.row) {
    ESP_LOGE(TAG, "display allocation failed");
    release();
    return;
  }
  memset(state.row, 0, 2 * DMA_BYTES);
  for (unsigned value = 0; value < 256; ++value) {
    uint8_t mask = 0;
    for (unsigned p = 0; p < 4; ++p)
      if (!(value & (3 << (p * 2)))) mask |= 3 << (p * 2);
    state.unchangedLut[value] = mask;
  }
  state.previous = state.memory;
  state.target = state.previous + FRAME_BYTES;
  state.lsb = state.target + FRAME_BYTES;
  state.msb = state.lsb + PLANE_BYTES;
  state.transitions = reinterpret_cast<uint16_t*>(state.msb + PLANE_BYTES);
  memset(state.previous, 0xFF, 2 * FRAME_BYTES);
  memset(state.lsb, 0, 2 * PLANE_BYTES);
  // PaperS3 owns LCD_CAM exclusively. Use its i8080 data phase directly,
  // with one finite GDMA descriptor per row and software-controlled SPH.
  periph_module_enable(PERIPH_LCD_CAM_MODULE);
  periph_module_reset(PERIPH_LCD_CAM_MODULE);
  state.peripheral = true;
  lcd_ll_reset(&LCD_CAM);
  lcd_ll_enable_clock(&LCD_CAM, true);
  lcd_ll_select_clk_src(&LCD_CAM, LCD_CLK_SRC_PLL160M);
  lcd_ll_set_group_clock_coeff(&LCD_CAM, 4, 0, 0);
  lcd_ll_set_clock_idle_level(&LCD_CAM, false);
  lcd_ll_set_pixel_clock_edge(&LCD_CAM, false);
  lcd_ll_enable_rgb_mode(&LCD_CAM, false);
  lcd_ll_enable_rgb_yuv_convert(&LCD_CAM, false);
  lcd_ll_enable_auto_next_frame(&LCD_CAM, false);
  lcd_ll_set_dma_read_stride(&LCD_CAM, 8);
  lcd_ll_reverse_dma_data_bit_order(&LCD_CAM, false);
  lcd_ll_reverse_wire_bit_order(&LCD_CAM, false);
  lcd_ll_swap_dma_data_byte_order(&LCD_CAM, false);
  lcd_ll_enable_output_always_on(&LCD_CAM, false);
  lcd_ll_set_blank_cycles(&LCD_CAM, 1, 1);
  lcd_ll_set_pixel_clock_prescale(&LCD_CAM, 2);
  lcd_ll_set_phase_cycles(&LCD_CAM, 0, 0, DMA_BYTES);
  for (unsigned i = 0; i < 8; ++i) esp_rom_gpio_connect_out_signal(pins[i], LCD_DATA_OUT0_IDX + i, false, false);
  esp_rom_gpio_connect_out_signal(PAPERS3_EP_CL, LCD_PCLK_IDX, false, false);
  pin(PAPERS3_EP_SPH, true);
  gdma_channel_alloc_config_t channel{};
  channel.direction = GDMA_CHANNEL_DIRECTION_TX;
  if (gdma_new_ahb_channel(&channel, &state.dma) != ESP_OK) {
    release();
    return;
  }
  if (gdma_connect(state.dma, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0)) != ESP_OK) {
    release();
    return;
  }
  state.connected = true;
  gdma_strategy_config_t strategy{};
  strategy.owner_check = true;
  strategy.auto_update_desc = false;
  if (gdma_apply_strategy(state.dma, &strategy) != ESP_OK) {
    release();
    return;
  }
  for (unsigned i = 0; i < 2; ++i) {
    auto& descriptor = state.descriptors[i];
    descriptor.dw0.size = DMA_BYTES;
    descriptor.dw0.length = DMA_BYTES;
    descriptor.dw0.suc_eof = 1;
    descriptor.buffer = state.row + i * DMA_BYTES;
  }
  state.ready = true;
}

void PaperS3Driver::deepSleep(EpdBus&) {
  state.pending = false;
  power(false);
}
void PaperS3Driver::display(EpdBus&, const uint8_t* fb, const uint8_t*, RefreshMode mode, bool turnOff) {
  if (!state.ready || !fb) return;
  loadBw(fb);
  state.mode = mode;
  state.pending = false;
  commit(false, turnOff);
}
GrayscaleCapabilities PaperS3Driver::grayscaleCapabilities(GrayscaleMode mode) const {
  if (!state.ready || mode != GrayscaleMode::Overlay) return {};
  return {GrayscaleEncoding::OverlayMasks, GrayscaleBase::Combined, true, false, false};
}
void PaperS3Driver::displayGrayscaleBase(EpdBus&, const uint8_t* fb, RefreshMode mode, bool turnOff) {
  if (!state.ready || !fb) return;
  loadBw(fb);
  memset(state.lsb, 0, 2 * PLANE_BYTES);
  state.mode = mode;
  state.turnOff = turnOff;
  state.pending = true;
}
void PaperS3Driver::copyGrayscaleLsb(EpdBus&, const uint8_t* plane) {
  if (state.ready && plane) memcpy(state.lsb, plane, PLANE_BYTES);
}
void PaperS3Driver::copyGrayscaleMsb(EpdBus&, const uint8_t* plane) {
  if (state.ready && plane) memcpy(state.msb, plane, PLANE_BYTES);
}
void PaperS3Driver::writeGrayscalePlaneStrip(EpdBus&, GrayPlane plane, const uint8_t* rows, uint16_t y,
                                             uint16_t count) {
  if (!state.ready || !rows || y >= HEIGHT || count > HEIGHT - y) return;
  memcpy((plane == GrayPlane::Lsb ? state.lsb : state.msb) + y * (WIDTH / 8), rows, count * (WIDTH / 8));
}
void PaperS3Driver::displayGray(EpdBus&, const uint8_t*, bool turnOff, const unsigned char*, bool) {
  if (!state.ready) return;
  for (size_t i = 0; i < PLANE_BYTES; ++i) {
    for (unsigned p = 0; p < 8; ++p) {
      const uint8_t mask = 0x80 >> p;
      const bool l = state.lsb[i] & mask, m = state.msb[i] & mask;
      if (!l && !m) continue;
      const auto index = i * 2 + p / 4;
      const unsigned shift = 6 - 2 * (p % 4);
      state.target[index] = (state.target[index] & ~(3 << shift)) | ((m && !l ? 2 : 1) << shift);
    }
  }
  commit(true, turnOff || state.turnOff);
}
void PaperS3Driver::cleanupGrayscaleBuffers(EpdBus&, const uint8_t* bw) {
  if (!state.pending) return;
  if (!bw) {
    state.pending = false;
    return;
  }
  loadBw(bw);
  commit(false, state.turnOff);
}
PaperS3Driver& paperS3Driver() {
  static PaperS3Driver driver;
  return driver;
}
}  // namespace freeink
#endif
