#!/usr/bin/env python3
"""Run the native PaperS3 driver against delayed, recording DMA and fault injection."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
SOURCE = HERE.parents[1] / 'src'
with tempfile.TemporaryDirectory(prefix='papers3-driver-') as directory:
    root = Path(directory)
    def write(name, text):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
    for name in ('PaperS3Driver.cpp', 'PaperS3Driver.h', 'PaperS3LovyanWaveforms.h', 'PaperS3NativeWaveform.h', 'PanelDriver.h'):
        write('driver/' + name, (SOURCE / 'driver' / name).read_text().replace('../../include/GrayscaleCapabilities.h', '../GrayscaleCapabilities.h'))
    shutil.copytree(SOURCE / 'driver/experimental/fast', root / 'driver/experimental/fast')
    shutil.copy2(SOURCE.parent / 'include/GrayscaleCapabilities.h', root)
    pins = SOURCE.parents[2] / 'hardware/BoardPaperS3/include/BoardPaperS3Pins.h'
    shutil.copy2(pins, root)
    write('BoardConfig.h', '#define FREEINK_DRIVER_PAPERS3 1\n')
    write('bus/EpdBus.h', '#pragma once\nnamespace freeink { enum class BusyPolarity { ActiveLow }; class EpdBus {}; }\n')
    write('mock.h', r'''#pragma once
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <array>
#define IRAM_ATTR
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_INTERNAL 8
#define GPIO_MODE_OUTPUT 1
#define SIG_GPIO_OUT_IDX 256
#define LCD_CLK_SRC_PLL160M 1
#define ESP_OK 0
#define pdTRUE 1
#define pdFALSE 0
#define pdMS_TO_TICKS(x) (x)
using BaseType_t=int;
using gpio_num_t=int;
using esp_lcd_i80_bus_handle_t=void*;
using esp_lcd_panel_io_handle_t=void*;
using SemaphoreHandle_t=void*;
struct esp_lcd_panel_io_event_data_t {};
using Done=bool(*)(void*,esp_lcd_panel_io_event_data_t*,void*);
struct esp_lcd_i80_bus_config_t { int clk_src,dc_gpio_num,wr_gpio_num,bus_width; size_t max_transfer_bytes; int data_gpio_nums[8]; };
struct esp_lcd_panel_io_i80_config_t { int cs_gpio_num,pclk_hz,trans_queue_depth,lcd_cmd_bits,lcd_param_bits; Done on_color_trans_done; struct { unsigned pclk_idle_low:1; } flags; };
using portMUX_TYPE=int;
#define portMUX_INITIALIZER_UNLOCKED 0
inline void portENTER_CRITICAL(int*) {}
inline void portEXIT_CRITICAL(int*) {}
inline unsigned cycleCount=0;
inline unsigned esp_cpu_get_cycle_count(){return cycleCount+=24;}
inline int GPIO=0,pins[64]{},allocations=0,failAllocation=0,busError=0,ioError=0;
inline bool timeout=false,submitError=false;
inline unsigned submissions=0;
inline Done callback=nullptr;
inline const uint8_t* inFlight=nullptr;
inline std::vector<uint8_t> snapshot;
inline std::vector<std::array<uint8_t,4>> firstRows;
inline uint64_t now=0;
inline unsigned scanPauses=0;
inline unsigned nativePauses=0;
inline void delayMicroseconds(unsigned n) {now+=n;if(n==250)++scanPauses;if(n==4000)++nativePauses;}
inline void delay(unsigned n) {now+=n*1000;}
inline uint64_t esp_timer_get_time() {return now+=10;}
inline unsigned getCpuFrequencyMhz() {return 240;}
inline void taskYIELD() {}
inline void vTaskDelay(unsigned n) { now+=n*1000; }
inline void gpio_set_level(int pin,int value) {pins[pin]=value;}
inline void gpio_set_direction(int,int) {}
inline void gpio_reset_pin(int) {}
inline void esp_rom_gpio_connect_out_signal(int,int,bool,bool) {}
inline void gpio_ll_set_level(int*,int pin,int value) {pins[pin]=value;}
inline void* heap_caps_malloc(size_t n,int) {if(++allocations==failAllocation)return nullptr;return malloc(n);}
inline void heap_caps_free(void* p) {free(p);}
using gdma_channel_handle_t=void*;
struct dma_descriptor_t {
 struct {unsigned size=0,length=0,suc_eof=0,owner=0;} dw0;
 void* buffer=nullptr;dma_descriptor_t* next=nullptr;
};
struct gdma_channel_alloc_config_t {int direction;};
struct gdma_strategy_config_t {bool owner_check,auto_update_desc;};
#define GDMA_CHANNEL_DIRECTION_TX 1
#define DMA_DESCRIPTOR_BUFFER_OWNER_DMA 1
#define PERIPH_LCD_CAM_MODULE 1
#define GDMA_TRIG_PERIPH_LCD 1
#define GDMA_MAKE_TRIGGER(a,b) (a)
#define LCD_DATA_OUT0_IDX 0
#define LCD_PCLK_IDX 8
struct Start {
 bool active=false;
 void operator=(bool value) {active=value;}
 operator bool() {
  if(!active)return false;
  if(timeout)return true;
  assert(inFlight && memcmp(snapshot.data(),inFlight,snapshot.size())==0);
  inFlight=nullptr;active=false;return false;
 }
};
struct Lcd {struct {Start lcd_start;} lcd_user;} LCD_CAM;
inline bool peripheral=false,dmaAllocated=false,dmaConnected=false;
inline int strategyError=0,prescale=0,rowBytes=0;
inline void periph_module_enable(int) {assert(!peripheral);peripheral=true;}
inline void periph_module_disable(int) {assert(peripheral);peripheral=false;}
inline void periph_module_reset(int) {}
inline void lcd_ll_reset(Lcd*) {}
inline void lcd_ll_enable_clock(Lcd*,bool) {}
inline void lcd_ll_select_clk_src(Lcd*,int source) {assert(source==LCD_CLK_SRC_PLL160M);}
inline void lcd_ll_set_group_clock_coeff(Lcd*,int n,int a,int b) {assert(n==4 && !a && !b);}
inline void lcd_ll_set_pixel_clock_prescale(Lcd*,int n) {assert(n==1 || n==2);prescale=n;}
inline void lcd_ll_set_clock_idle_level(Lcd*,bool) {}
inline void lcd_ll_set_pixel_clock_edge(Lcd*,bool) {}
inline void lcd_ll_enable_rgb_mode(Lcd*,bool) {}
inline void lcd_ll_enable_rgb_yuv_convert(Lcd*,bool) {}
inline void lcd_ll_enable_auto_next_frame(Lcd*,bool) {}
inline void lcd_ll_set_dma_read_stride(Lcd*,int n) {assert(n==8);}
inline void lcd_ll_reverse_dma_data_bit_order(Lcd*,bool) {}
inline void lcd_ll_reverse_wire_bit_order(Lcd*,bool) {}
inline void lcd_ll_swap_dma_data_byte_order(Lcd*,bool) {}
inline void lcd_ll_enable_output_always_on(Lcd*,bool value) {assert(!value);}
inline void lcd_ll_set_phase_cycles(Lcd*,int c,int d,int n) {assert(!c && !d && (n==248 || n==256));rowBytes=n;}
inline void lcd_ll_set_blank_cycles(Lcd*,int,int) {}
inline void lcd_ll_fifo_reset(Lcd*) {assert(!LCD_CAM.lcd_user.lcd_start.active);}
inline void lcd_ll_start(Lcd*) {assert(inFlight);LCD_CAM.lcd_user.lcd_start=true;}
inline void lcd_ll_stop(Lcd*) {LCD_CAM.lcd_user.lcd_start=false;}
inline int gdma_new_ahb_channel(const gdma_channel_alloc_config_t* c,void** p) {
 assert(c->direction==GDMA_CHANNEL_DIRECTION_TX && !dmaAllocated);
 if(busError)return -1;dmaAllocated=true;*p=reinterpret_cast<void*>(1);return 0;
}
inline int gdma_connect(void*,int) {if(ioError)return -1;dmaConnected=true;return 0;}
inline int gdma_apply_strategy(void*,const gdma_strategy_config_t* c) {
 assert(c->owner_check && !c->auto_update_desc);return strategyError;
}
inline void gdma_stop(void*) {inFlight=nullptr;}
inline void gdma_disconnect(void*) {assert(dmaConnected);dmaConnected=false;}
inline void gdma_del_channel(void*) {assert(dmaAllocated && !dmaConnected);dmaAllocated=false;}
inline int gdma_start(void*,intptr_t address) {
  auto* d=reinterpret_cast<dma_descriptor_t*>(address);
  assert(!inFlight && d->dw0.size==rowBytes && d->dw0.length==rowBytes);
  assert(d->dw0.owner==DMA_DESCRIPTOR_BUFFER_OWNER_DMA && d->dw0.suc_eof && !d->next);
  if(submitError)return -1;
  inFlight=static_cast<const uint8_t*>(d->buffer);snapshot.assign(inFlight,inFlight+rowBytes);
  if(submissions%540==0)firstRows.push_back({inFlight[0],inFlight[1],inFlight[2],inFlight[3]});
  for(size_t i=240;i<rowBytes;++i)assert(inFlight[i]==0);
  ++submissions;return 0;
}

''')
    for name in ('Arduino.h','esp_heap_caps.h','esp_lcd_panel_io.h','esp_log.h','esp_timer.h','esp_rom_gpio.h','soc/gpio_sig_map.h','driver/gpio.h','freertos/FreeRTOS.h','freertos/semphr.h','freertos/task.h','hal/gpio_ll.h','soc/gpio_struct.h','esp_private/gdma.h','esp_private/periph_ctrl.h','hal/dma_types.h','hal/lcd_ll.h','soc/lcd_cam_struct.h'):
        write(name, '#include <mock.h>\n')
    write('test.cpp', r'''
#include "driver/PaperS3Driver.cpp"
#include <iostream>
using namespace freeink;
int main() {
  EpdBus bus; auto& driver=paperS3Driver();
  namespace lovyan=papers3::lovyan;
  namespace native=papers3::native;
  assert(selectedWaveform==PaperS3Waveform::Native); // default waveform
  static_assert(lovyan::DIFFERENTIAL_STEPS==29);
  static_assert(sizeof(lovyan::TEXT)/sizeof(lovyan::TEXT[0])==32);
  // Shorten only the repeated settling tail, never an active drive phase.
  for(size_t s=lovyan::TEXT_ACTIVE_STEPS;s<31;++s)assert(lovyan::TEXT[s]==~0u);
  // The packed-path checks below exercise the LovyanGFX reference tables.
  driver.setWaveform(PaperS3Waveform::LovyanGfx);
  for(int allocation=1;allocation<=2;++allocation) {
    allocations=0;failAllocation=allocation;driver.begin(bus);
    assert(!state.ready && !state.memory && !pins[PAPERS3_EP_PWR]);
  }
  failAllocation=0;busError=1;driver.begin(bus);assert(!state.ready && !state.memory);
  busError=0;ioError=1;driver.begin(bus);assert(!state.ready && !state.memory);
  ioError=0;strategyError=1;driver.begin(bus);assert(!state.ready && !state.memory && !peripheral && !dmaAllocated);
  strategyError=0;driver.begin(bus);assert(state.ready);
  // Exhaustively compare packed transition suppression against per-pixel codes.
  for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b) {
    uint8_t expected=0;
    for(unsigned p=0;p<4;++p) {
      const unsigned shift=6-2*p;
      const unsigned old=(a>>shift)&3,next=(b>>shift)&3;
      expected |= (old==next ? 3 : ((lovyan::TEXT[5]>>(next*10))&3))<<shift;
    }
    assert(lovyan::driveByte(a,b,lovyan::TEXT[5],false,false)==expected);
    assert((lovyan::driveByte(b,b,lovyan::TEXT[5],false,true)|state.unchangedLut[a^b])==expected);
  }
  std::vector<uint8_t> bw(64800,255),lsb(64800,0),msb(64800,0);
  bw[0]=0x1f;lsb[0]=0x40;msb[0]=0x60;
  driver.displayGrayscaleBase(bus,bw.data(),RefreshMode::Fast,true);
  driver.copyGrayscaleLsb(bus,lsb.data());driver.copyGrayscaleMsb(bus,msb.data());
  assert(submissions==0); // staging cannot flash a preliminary B/W page
  driver.displayGray(bus,bw.data(),true,nullptr,false);
  assert(state.previous[0]==0x1b); // black,dark,light,white
  assert(submissions>0 && !state.pending && !pins[PAPERS3_EP_PWR]);
  assert(scanPauses==submissions/540);
  for(auto byte:snapshot)assert(byte==0); // final neutral scan before power-off
  const auto first=submissions;
  driver.cleanupGrayscaleBuffers(bus,bw.data());assert(submissions==first);
  driver.displayGrayscaleBase(bus,bw.data(),RefreshMode::Fast,true);
  driver.copyGrayscaleLsb(bus,lsb.data());driver.copyGrayscaleMsb(bus,msb.data());
  driver.displayGray(bus,bw.data(),true,nullptr,false);assert(submissions==first); // unchanged frame
  driver.displayGrayscaleBase(bus,bw.data(),RefreshMode::Fast,true);
  driver.cleanupGrayscaleBuffers(bus,nullptr);assert(submissions==first && !state.pending);
  driver.displayGrayscaleBase(bus,bw.data(),RefreshMode::Fast,true);
  driver.writeGrayscalePlaneStrip(bus,GrayPlane::Lsb,lsb.data(),539,2);assert(state.lsb[539*120]==0);
  driver.cleanupGrayscaleBuffers(bus,bw.data());assert(state.previous[0]==0x03);
  // The public overlay API also permits a separately presented B/W base.
  driver.copyGrayscaleLsb(bus,lsb.data());driver.copyGrayscaleMsb(bus,msb.data());
  driver.displayGray(bus,bw.data(),true,nullptr,false);
  assert(state.previous[0]==0x1b);
  // All 16 old/new tone pairs through the actual packed DMA path.
  // Pure B/W transitions must match FAST, gray transitions the original
  // erase/text sequence, and unchanged pixels must not receive +/- drive.
  for(unsigned old=0;old<4;++old){state.previous[old]=old*0x55;state.target[old]=0x1b;}
  state.mode=RefreshMode::Fast;
  firstRows.clear();
  commit(true,true);
  assert(firstRows.size()==lovyan::DIFFERENTIAL_STEPS);
  for(size_t step=0;step<firstRows.size();++step)for(unsigned old=0;old<4;++old)for(unsigned next=0;next<4;++next){
    unsigned expected;
    if(step==firstRows.size()-1)expected=0;
    else if(old==next)expected=3;
    else if((old==0||old==3)&&(next==0||next==3))
      expected=step<10?lovyan::drive(lovyan::FAST[step],next):0;
    else expected=step<4?lovyan::drive(lovyan::ERASE[step],old):lovyan::drive(lovyan::TEXT[step-4],next);
    assert(((firstRows[step][old]>>(6-2*next))&3)==expected);
  }
  assert(!pins[PAPERS3_EP_PWR]);
  // Switch waveforms at runtime: buffers are kept, the first page after a
  // switch is a full clear, then the waveform's differential plan is used.
  const auto* memory=state.memory;
  const auto allocated=allocations;
  for(auto waveform : {PaperS3Waveform::Native,PaperS3Waveform::FastEpd,PaperS3Waveform::LovyanGfx,PaperS3Waveform::Native}) {
    driver.setWaveform(waveform);
    assert(!state.baseline && !state.pending);
    assert(state.memory==memory && allocations==allocated);
    const bool fast=waveform==PaperS3Waveform::FastEpd;
    assert(prescale==(fast?1:2) && rowBytes==(fast?256:248));
    const auto before=submissions;
    const auto pausesBefore=nativePauses;
    firstRows.clear();
    driver.display(bus,bw.data(),nullptr,RefreshMode::Fast,true);
    assert(state.baseline && submissions>before && !pins[PAPERS3_EP_PWR]);
    if(waveform==PaperS3Waveform::Native) {
      // Black, then white, then a float scan, then the transition plan from white.
      const size_t clear=2*native::BASE_STEPS+1;
      assert(submissions-before==(clear+native::STEPS)*540);
      assert(nativePauses-pausesBefore==clear+native::STEPS);
      for(size_t i=0;i<clear;++i) {
        const uint8_t code=i<native::BASE_STEPS?0x55:i<2*native::BASE_STEPS?0xaa:0;
        for(auto byte:firstRows[i])assert(byte==code);
      }
    }
    if(fast)assert(submissions-before==papers3::fast::FULL_STEPS*540);
    driver.setWaveform(waveform);
    assert(state.baseline); // unchanged setting never forces repeated clears
    const auto after=submissions;
    driver.display(bus,bw.data(),nullptr,RefreshMode::Fast,true);
    assert(submissions==after);
    bw[0]^=0xff;
    driver.display(bus,bw.data(),nullptr,RefreshMode::Fast,true);
    const size_t steps=waveform==PaperS3Waveform::Native?native::STEPS:fast?papers3::fast::DIFFERENTIAL_STEPS:10;
    assert(submissions-after==steps*540);
  }
  driver.setWaveform(static_cast<PaperS3Waveform>(255));
  assert(selectedWaveform==PaperS3Waveform::Native);
  bw[0]=0; // ensure the timeout test has changed pixels
  timeout=true;bw[0]=255;
  const auto beforeTimeout=state.previous[0];
  driver.display(bus,bw.data(),nullptr,RefreshMode::Fast,true);
  assert(!state.ready && state.previous[0]==beforeTimeout && !pins[PAPERS3_EP_PWR]);
  const auto failed=submissions;driver.display(bus,bw.data(),nullptr,RefreshMode::Fast,true);assert(submissions==failed);
  inFlight=nullptr;timeout=false;release();driver.begin(bus);
  submitError=true;driver.display(bus,bw.data(),nullptr,RefreshMode::Full,true);
  assert(!state.ready && !pins[PAPERS3_EP_PWR]);
  release();
  std::cout<<"PaperS3: packed transitions, one grayscale commit, unchanged-frame skip, DMA ownership, OOM and bus failures passed\n";
}
''')
    exe=root/'test'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(root),str(root/'test.cpp'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

with tempfile.TemporaryDirectory(prefix='papers3-waveform-') as directory:
    exe = Path(directory) / 'waveform'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(SOURCE), str(HERE / 'test_papers3_native_waveform.cpp'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    fast = Path(directory) / 'fastepd'
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(HERE / 'fast/test.cpp'), '-o', str(fast)], check=True)
    subprocess.run([str(fast)], check=True)
