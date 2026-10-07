#include "PaperS3GatePulse.h"

#if defined(FREEINK_DEVICE_PAPERS3) && FREEINK_DEVICE_PAPERS3
#include <sdkconfig.h>
#if !CONFIG_IDF_TARGET_ESP32S3
#error "PaperS3 gate pulses require ESP32-S3"
#endif
#include <driver/gpio.h>
#include <esp_private/periph_ctrl.h>
#include <esp_rom_gpio.h>
#include <esp_timer.h>
#include <hal/rmt_ll.h>
#include <soc/gpio_sig_map.h>
#include <soc/rmt_periph.h>
#include <soc/rmt_struct.h>

struct PaperS3RmtMemory {
  struct {
    volatile uint32_t words[SOC_RMT_MEM_WORDS_PER_CHANNEL];
  } channel[SOC_RMT_CHANNELS_PER_GROUP];
};
extern "C" PaperS3RmtMemory RMTMEM;

namespace freeink::papers3::epdiy {
namespace {
constexpr uint32_t CHANNEL = 0;
constexpr uint32_t EVENTS = RMT_LL_EVENT_TX_MASK(CHANNEL) | RMT_LL_EVENT_TX_ERROR(CHANNEL);
GatePulse* owner = nullptr;
}

bool GatePulse::acquire(int gpio) {
  if (acquired || owner || !GPIO_IS_VALID_OUTPUT_GPIO(gpio)) return false;
  // PaperS3 has no other RMT consumer. Explicit ownership excludes a second
  // native display instance; this does not reserve an IDF-driver channel.
  owner = this;
  pin = gpio;
  periph_module_enable(PERIPH_RMT_MODULE);
  periph_module_reset(PERIPH_RMT_MODULE);
  rmt_ll_enable_periph_clock(&RMT, true);
  rmt_ll_mem_force_power_on(&RMT);
  rmt_ll_enable_mem_access_nonfifo(&RMT, true);
  // PaperS3's 40 MHz crystal / 4 = 100 ns; independent of CPU/APB scaling.
  rmt_ll_set_group_clock_src(&RMT, CHANNEL, RMT_CLK_SRC_XTAL, 1, 0, 0);
  rmt_ll_enable_group_clock(&RMT, true);
  rmt_ll_tx_set_channel_clock_div(&RMT, CHANNEL, 4);
  rmt_ll_tx_set_mem_blocks(&RMT, CHANNEL, 1);
  // Channel 0 has no DMA; even disabling DMA via the LL helper asserts on S3.
  rmt_ll_tx_enable_loop(&RMT, CHANNEL, false);
  rmt_ll_tx_enable_wrap(&RMT, CHANNEL, false);
  rmt_ll_tx_enable_carrier_modulation(&RMT, CHANNEL, false);
  rmt_ll_tx_fix_idle_level(&RMT, CHANNEL, 0, true);
  rmt_ll_enable_interrupt(&RMT, EVENTS, false);
  rmt_ll_clear_interrupt_status(&RMT, EVENTS);
  gpio_set_level(static_cast<gpio_num_t>(pin), 0);
  esp_rom_gpio_connect_out_signal(pin, rmt_periph_signals.groups[0].channels[CHANNEL].tx_sig, false, false);
  acquired = true;
  return true;
}

bool GatePulse::start(uint16_t highTenthsUs, uint16_t lowTenthsUs) {
  if (!acquired || pending || !highTenthsUs || highTenthsUs > 32767 || !lowTenthsUs || lowTenthsUs > 32767)
    return false;
  // One finite high/low item followed by the zero-duration terminator.
  RMTMEM.channel[CHANNEL].words[0] = highTenthsUs | (1u << 15) | (uint32_t(lowTenthsUs) << 16);
  RMTMEM.channel[CHANNEL].words[1] = 0;
  rmt_ll_clear_interrupt_status(&RMT, EVENTS);
  rmt_ll_tx_reset_pointer(&RMT, CHANNEL);
  pending = true;
  rmt_ll_tx_start(&RMT, CHANNEL);
  return true;
}

bool GatePulse::wait(uint32_t timeoutUs) {
  if (!acquired) return false;
  if (!pending) return true;
  const auto started = esp_timer_get_time();
  while (true) {
    const auto status = rmt_ll_tx_get_interrupt_status_raw(&RMT, CHANNEL);
    if (status & RMT_LL_EVENT_TX_ERROR(CHANNEL)) break;
    if (status & RMT_LL_EVENT_TX_DONE(CHANNEL)) {
      pending = false;
      return true;
    }
    if (esp_timer_get_time() - started >= timeoutUs) break;
  }
  release();
  return false;
}

void GatePulse::release() {
  if (!acquired) return;
  rmt_ll_tx_stop(&RMT, CHANNEL);
  gpio_set_level(static_cast<gpio_num_t>(pin), 0);
  esp_rom_gpio_connect_out_signal(pin, SIG_GPIO_OUT_IDX, false, false);
  rmt_ll_clear_interrupt_status(&RMT, EVENTS);
  rmt_ll_enable_group_clock(&RMT, false);
  rmt_ll_mem_force_power_off(&RMT);
  rmt_ll_enable_periph_clock(&RMT, false);
  periph_module_disable(PERIPH_RMT_MODULE);
  acquired = false;
  pending = false;
  owner = nullptr;
}
}  // namespace freeink::papers3::epdiy
#endif
