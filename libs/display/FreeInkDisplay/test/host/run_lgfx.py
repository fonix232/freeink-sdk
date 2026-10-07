#!/usr/bin/env python3
"""Exercise the real LGFX adapter with a recording graphics device (no panel emulation)."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / "src"
with tempfile.TemporaryDirectory(prefix="lgfx-test-") as directory:
    root = Path(directory)
    for folder in ("driver", "bus", "lgfx/v1/platforms/esp32"):
        (root / folder).mkdir(parents=True)
    for name in ("LgfxEpdDriver.cpp", "LgfxEpdDriver.h", "PanelDriver.h"):
        shutil.copy2(SOURCE / "driver" / name, root / "driver" / name)
    for name in ("GrayscaleCapabilities.h", "LgfxEpdConfig.h"):
        shutil.copy2(SOURCE.parent / "include" / name, root / name)
    panel = root / "driver/PanelDriver.h"
    panel.write_text(panel.read_text().replace("../../include/GrayscaleCapabilities.h", "../GrayscaleCapabilities.h"))
    (root / "Arduino.h").write_text('#pragma once\n#include <cstdint>\n#include <cstddef>\n')
    (root / "BoardConfig.h").write_text('''#pragma once
namespace BoardConfig {
inline struct { unsigned short displayWidth=8, displayHeight=2; } ACTIVE;
inline bool paper=true;
inline bool isM5PaperS3() { return paper; }
}
''')
    (root / "esp_heap_caps.h").write_text('''#pragma once
#include <cstdlib>
#define MALLOC_CAP_SPIRAM 1
inline void* heap_caps_malloc(size_t n, int) { return malloc(n); }
''')
    (root / "bus/EpdBus.h").write_text('''#pragma once
namespace freeink {
enum class BusyPolarity { ActiveLow };
class EpdBus {};
}
''')
    (root / "M5GFX.h").write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace lgfx {
namespace epd_mode { enum epd_mode_t { epd_fast, epd_text, epd_fastest }; }
enum class color_depth_t { grayscale_8bit };
inline unsigned pushes=0, sleeps=0;
inline bool initCleared=false;
inline epd_mode::epd_mode_t pushedMode=epd_mode::epd_fast;
inline std::vector<uint8_t> pixels;
class Bus_EPD {
public:
  struct Config {
    uint32_t bus_speed=0;
    int pin_data[8]{},pin_pwr=0,pin_sph=0,pin_spv=0,pin_oe=0,pin_le=0,pin_cl=0,pin_ckv=0,bus_width=0;
  };
  Config config() { return {}; }
  void config(Config) {}
  virtual bool init() { return true; }
  virtual bool powerControl(bool) { return true; }
  void wait() {}
protected:
  bool _pwr_on=false;
};
class Panel_EPD {
public:
  struct Detail {
    unsigned line_padding=0;
    const uint32_t *lut_quality=nullptr,*lut_text=nullptr,*lut_fast=nullptr,*lut_fastest=nullptr;
    size_t lut_quality_step=0,lut_text_step=0,lut_fast_step=0,lut_fastest_step=0;
  };
  struct Config { unsigned memory_width=0,panel_width=0,memory_height=0,panel_height=0,offset_rotation=0,offset_x=0,offset_y=0; bool bus_shared=false; };
  void setBus(Bus_EPD*) {}
  Detail config_detail() { return {}; }
  void config_detail(Detail) {}
  Config config() { return {}; }
  void config(Config) {}
};
class LGFX_Device {
public:
  epd_mode::epd_mode_t mode=epd_mode::epd_fast;
  void setPanel(Panel_EPD*) {}
  void setRotation(unsigned) {}
  void setEpdMode(epd_mode::epd_mode_t m) { mode=m; }
  void waitDisplay() {}
  void setAutoDisplay(bool) {}
  uint32_t color888(uint8_t r,uint8_t g,uint8_t b) { return (r<<16)|(g<<8)|b; }
  void fillScreen(uint32_t) {}
  void display() {}
  void sleep() { ++sleeps; }
protected:
  bool init_impl(bool, bool clear) { initCleared=clear; return true; }
};
class LGFX_Sprite {
  LGFX_Device* device;
  std::vector<uint8_t> buffer;
public:
  explicit LGFX_Sprite(LGFX_Device* d):device(d) {}
  void setPsram(bool) {}
  void setColorDepth(color_depth_t) {}
  void createSprite(unsigned w,unsigned h) { buffer.resize(w*h); }
  void* getBuffer() { return buffer.data(); }
  void pushSprite(int,int) { ++pushes; pushedMode=device->mode; pixels=buffer; }
};
}
''')
    for name in ("Bus_EPD.h", "Panel_EPD.hpp"):
        (root / "lgfx/v1/platforms/esp32" / name).write_text('#include <M5GFX.h>\n')
    exe = root / "test_lgfx"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra",
                    "-Wno-unused-parameter", "-DFREEINK_DRIVER_LGFX_EPD=1", "-DFREEINK_LGFX_EPD_CONFIG=testLgfxConfig",
                    "-I"+str(root), str(HERE / "test_lgfx.cpp"),
                    str(root / "driver/LgfxEpdDriver.cpp"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
