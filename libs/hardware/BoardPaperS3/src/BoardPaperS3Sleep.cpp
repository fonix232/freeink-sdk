#include <BoardPaperS3.h>
#include <esp_sleep.h>
#include <esp_timer.h>

namespace BoardPaperS3 {
uint32_t idleSleep(uint32_t milliseconds) {
  if (!milliseconds || milliseconds > 50 || usbMillivolts() > 200) return 0;
  if (esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(milliseconds) * 1000) != ESP_OK) return 0;
  // Leave flash/PSRAM power policy at the SDK defaults; no forced power-down.
  const auto start = esp_timer_get_time();
  const auto result = esp_light_sleep_start();
  const auto elapsed = esp_timer_get_time() - start;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  return result == ESP_OK ? static_cast<uint32_t>(elapsed) : 0;
}
}  // namespace BoardPaperS3
