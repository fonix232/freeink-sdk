# PaperS3 native ED047TC1 driver

`FREEINK_DEVICE_PAPERS3=1` selects `PaperS3Driver`, which drives the panel
directly through the ESP32-S3 LCD_CAM peripheral and GDMA. No M5GFX/LovyanGFX
code is linked; LilyGo boards keep their own LovyanGFX adapter.

## What is actually sent

The ED047TC1 has no framebuffer controller. Every scan covers 540 rows; each
row sends 240 bytes of four two-bit electrode drive codes (0 float, 1 darken,
2 whiten), then eight padding bytes. SPH selects the source shift register, CL
shifts it, LE latches the row, CKV advances the gate driver, and SPV starts a
frame. OE/PWR enable the panel. A refresh is a sequence of such scans — a
waveform generator, not an SPI framebuffer upload.

## Transport

- The driver owns LCD_CAM and one AHB GDMA TX channel. Two static descriptors
  reference two alternating 248-byte internal DMA rows; the CPU converts the
  next row while the previous one transmits.
- Pixel clock PLL160M / 4 / 2 = 20 MHz. Each row primes the LCD FIFO for 4 us
  before starting the clock; SPH, LE, CKV and SPV are plain GPIOs.
- Every row's completion is checked before its buffer is reused. A transfer
  stalled for 100 ms stops the peripheral, powers the rails down and disables
  further refreshes; the buffers are kept so a late completion never reads
  freed memory. Initialization failures release the channel, peripheral and
  allocations.
- The rails are powered for each refresh only.

## Memory

Two packed 2-bit images (previous and target), two 1-bit gray selector planes
and packed old/new transition indices: 648,000 bytes of PSRAM, allocated once
in `begin()`. Internal RAM holds the two DMA rows and three 256-byte lookup
tables (phase drive, unchanged-pixel mask, two-pixel transition codes), so the
scan loop does no per-pixel dispatch.

## Waveforms

### Native (default)

`PaperS3NativeWaveform.h` plans each pixel transition separately, using the
four-level NORMAL pulse trains of EPD_Painter (Apache-2.0, see
`PaperS3-EPDPainter-LICENSE.txt` and `PaperS3-EPDPainter-NOTICE.txt`). Only
the data is used; none of its runtime is linked.

- All 16 old/new tone pairs are defined. White ↔ gray/black uses one 13-pass
  train, five nonwhite direct transitions use the upstream trains, and dark
  gray → light gray (whose direct calibration did not converge upstream) uses
  a 13-pass removal followed by a 13-pass application.
- Every refresh emits 27 scans: up to 26 active steps, then one all-float
  scan. Unchanged pixels float throughout.
- Each scan is followed by a 4 ms pause, matching the timing the trains were
  calibrated with.
- A forced refresh (first page, a waveform change, or a Full/Half request)
  first drives the whole panel black for 13 scans, then white for 13, then
  floats once, and plans the page from that known white.
- Charge sums equal the target potential minus the old one for every
  transition, so closed tone cycles have zero nominal net charge. This is a
  digital invariant, not a guarantee of optical neutrality, and no
  temperature compensation is applied.

### LovyanGFX reference

`PaperS3LovyanWaveforms.h` keeps the drive tables of LovyanGFX's `Panel_EPD`
(bundled in M5GFX 0.2.29; BSD-2-Clause, see `PaperS3-LovyanGFX-LICENSE.txt`)
for comparison. Four tones use levels 0, 5, 10 and 15 of those tables.

- Full and Half refreshes run ERASE (old tones) followed by TEXT (new tones)
  on every pixel.
- Black/white fast refreshes run FAST on changed pixels only.
- Fast grayscale refreshes select a sequence per pixel: pure black/white
  changes use FAST, transitions involving gray use ERASE then the twelve
  active TEXT steps. The repeated settling tail is shortened to twelve scans,
  for 29 scans in total.
- Each scan is followed by a 250 us pause.

### Selecting a waveform

`FREEINK_PAPERS3_WAVEFORM` sets the boot waveform: `0` native (default), `1`
LovyanGFX. `PaperS3Driver::setWaveform()` switches at runtime; call it between
presentations under the render lock. The previous optical state does not carry
over, so the next refresh is a forced one. Unknown values select native.

## Refresh behaviour

- Fast refreshes leave unchanged pixels undriven, including gray ones; an
  identical frame causes no scan at all. Full and Half refresh every pixel.
- The B/W base and antialiasing masks are staged and presented in one
  refresh, so no intermediate B/W page is shown. Cleanup after a successful
  grayscale commit cannot present a stale page.

## Diagnostics

`lastScanTiming()` reports the last refresh's total time, row count and CPU
frequency. Building with `FREEINK_PAPERS3_PROFILE_SCANS=1` also splits it into
conversion, completion wait and submission time; per-row timers perturb the
waveform, so leave it off otherwise.

## Tests

`libs/display/FreeInkDisplay/test/host/run_papers3.py` runs the driver against a
delayed, recording DMA mock with ASan/UBSan: all 65,536 packed old/new
combinations, DMA buffer ownership, staging, unchanged-frame skipping, the
native forced-clear sequence, waveform switching without reallocation, and
allocation, bus, timeout and submission failures. It also exhausts the native
waveform's transitions, charge balance and packed rows. It does not model
pigment response or measure contrast.

## Validation

The native trains were calibrated by EPD_Painter on its own transport; with
LCD_CAM row timing here the reflectance is not automatically identical. Judge
a waveform on the physical panel (or with photometric measurements): gray-level
separation, black/white endpoints, reverse transitions, ghosting after repeated
alternating pages, and image retention across sleep. Framebuffer screenshots
show only what was submitted.

## Sources

- https://github.com/m5stack/M5GFX/tree/0.2.29/src/lgfx/v1/platforms/esp32
- https://github.com/tonywestonuk/EPD_Painter/blob/6335a149c5a9b3be4150880e0be64ef2529ba62a/src/EPD_Painter.cpp
- https://github.com/tonywestonuk/EPD_Painter/blob/6335a149c5a9b3be4150880e0be64ef2529ba62a/src/M5PaperS3_Trains.h
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/lcd/i80_lcd.html
