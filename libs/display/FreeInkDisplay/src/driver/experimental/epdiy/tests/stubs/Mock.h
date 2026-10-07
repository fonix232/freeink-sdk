#pragma once
#include <cstdint>
#include <cassert>
#define CONFIG_IDF_TARGET_ESP32S3 1
#define FREEINK_DEVICE_PAPERS3 1
#define FREEINK_PAPERS3_WAVEFORM 2
#define GPIO_IS_VALID_OUTPUT_GPIO(pin) ((pin) >= 0 && (pin) < 49)
#define SOC_RMT_MEM_WORDS_PER_CHANNEL 48
#define SOC_RMT_CHANNELS_PER_GROUP 8
#define RMT_LL_EVENT_TX_MASK(channel) 1u
#define RMT_LL_EVENT_TX_ERROR(channel) 2u
#define RMT_LL_EVENT_TX_DONE(channel) 1u
constexpr int PERIPH_RMT_MODULE = 1, RMT_CLK_SRC_XTAL = 3, SIG_GPIO_OUT_IDX = 256;
using gpio_num_t = int;
struct MockRmt {};
inline MockRmt RMT;
inline bool peripheralEnabled = false, autoComplete = true;
inline uint32_t rawStatus = 0;
inline int routedSignal = SIG_GPIO_OUT_IDX;
inline int64_t timeUs = 0;
struct MockSignals { struct Group { struct Channel { int tx_sig; } channels[8]; } groups[1]; };
inline MockSignals rmt_periph_signals = {{{{{42}}}}};
inline void periph_module_enable(int) { peripheralEnabled = true; }
inline void periph_module_disable(int) { peripheralEnabled = false; }
inline void periph_module_reset(int) {}
inline void rmt_ll_enable_periph_clock(MockRmt*, bool) {}
inline void rmt_ll_mem_force_power_on(MockRmt*) {}
inline void rmt_ll_mem_force_power_off(MockRmt*) {}
inline void rmt_ll_enable_mem_access_nonfifo(MockRmt*, bool) {}
inline void rmt_ll_set_group_clock_src(MockRmt*, unsigned, int, int, int, int) {}
inline void rmt_ll_enable_group_clock(MockRmt*, bool) {}
inline void rmt_ll_tx_set_channel_clock_div(MockRmt*, unsigned, int) {}
inline void rmt_ll_tx_set_mem_blocks(MockRmt*, unsigned, int) {}
inline void rmt_ll_tx_enable_dma(MockRmt*, unsigned channel, bool) { assert(channel == 3); }
inline void rmt_ll_tx_enable_loop(MockRmt*, unsigned, bool) {}
inline void rmt_ll_tx_enable_wrap(MockRmt*, unsigned, bool) {}
inline void rmt_ll_tx_enable_carrier_modulation(MockRmt*, unsigned, bool) {}
inline void rmt_ll_tx_fix_idle_level(MockRmt*, unsigned, int, bool) {}
inline void rmt_ll_enable_interrupt(MockRmt*, unsigned, bool) {}
inline void rmt_ll_clear_interrupt_status(MockRmt*, unsigned) { rawStatus = 0; }
inline void gpio_set_level(int, int) {}
inline void esp_rom_gpio_connect_out_signal(int, int signal, bool, bool) { routedSignal = signal; }
inline void rmt_ll_tx_reset_pointer(MockRmt*, unsigned) {}
inline void rmt_ll_tx_start(MockRmt*, unsigned) { rawStatus = autoComplete ? 1 : 0; }
inline void rmt_ll_tx_stop(MockRmt*, unsigned) {}
inline uint32_t rmt_ll_tx_get_interrupt_status_raw(MockRmt*, unsigned) { return rawStatus; }
inline int64_t esp_timer_get_time() { return timeUs++; }
