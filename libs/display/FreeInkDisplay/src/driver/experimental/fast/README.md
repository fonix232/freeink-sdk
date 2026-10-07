# PaperS3 FastEPD waveform backend

This allocation-free planner adapts FastEPD's PaperS3 waveforms to the SDK's
native scan transport. It does not link FastEPD or duplicate its DMA driver.

Source: [FastEPD.inl at 9a84c35](https://github.com/bitbank2/FastEPD/blob/9a84c35e52849f9f22d2a7ad5231ef501be49e04/src/FastEPD.inl).
Copyright 2024 BitBank Software, Inc.; Apache-2.0, included in LICENSE.txt.
The original distribution has no NOTICE file. Changes here transpose the
transition tables, select four matrix rows, and replace framebuffer allocation
and row emission with constexpr per-pixel/packed-byte planning.

## Implemented sequences

* Differential: five primary scans, one correction scan, one neutral scan.
  All sixteen old/new transitions follow `bbep2BppPartial` (lines 3358–3532).
  The light-gray to white correction remains `2` as in upstream code, despite
  upstream's inconsistent comment saying no second pass.
* Full: eight darkening scans, eight whitening scans, one neutral scan, eight
  target scans, one neutral scan. This follows `bbepFullUpdate` with CLEAR_FAST
  (lines 2927–2950) and `u8M5Matrix` rows 0/5/10/15 (lines 173–190).
  Full mode deliberately drives unchanged pixels to establish a known baseline.

`0` is neutral/discharge, `1` darkening, `2` whitening. Unchanged differential
pixels use upstream's neutral `0`, not the LovyanGFX reference's `3` code.
Tones are black=0, dark gray=1, light gray=2, white=3. Bytes are MS-pair first.

## Integration contract

`PaperS3Driver` integrates this as `PaperS3Waveform::FastEpd`
(`FREEINK_PAPERS3_WAVEFORM=2`) and applies the 40 MHz / 16-byte / row-start /
230 µs transport settings below while it is selected. The contract for any
other caller:

Include `PaperS3FastWaveform.h`. Iterate `stepCount(mode)` complete 960×540
scans, building the existing transition LUT from `drive(old, next, step, mode)`.
Use `Mode::Full` on initialization, after an interrupted update, or a requested
clean refresh; use `Mode::Differential` only with a valid previous framebuffer.
Do not skip the terminal neutral scan or substitute an unchanged-pixel mask
for full updates. Commit previous pixels only after every scan succeeds; on
failure turn rails off and mark panel history unknown.

FastEPD's PaperS3 profile specifies a 40 MHz source clock and 16 padding bytes
(lines 204–206), with a 230 µs pause after each scan. Row-start gate delays are
7/10/8/10/18/18 µs (`PaperS3RowControl`, lines 1330–1365); row stepping lowers
CKV and pulses LE. These are reference settings, not proof that any other
transport produces the same pixel pulse dose. The exported clock and pause
constants make the assumptions explicit: integration must select and measure
timing, and optical tests must validate contrast and ghosting before promoting
this experimental backend. Pass count alone cannot predict contrast or speed.

No heap allocations; waveform data are constexpr flash tables (64 bytes).
The planner has no framebuffer ownership, display-power control, ISR state, or
background work. The native driver retains those responsibilities.

## Host verification

Build `test/host/fast/test.cpp` relative to FreeInkDisplay with a C++17 compiler
and run it (ASan/UBSan recommended).
Tests cover source-table orientation, every packed byte pair in both modes,
neutral termination, full-refresh baseline/endpoint drive, and invalid inputs.
These are digital tests only; hardware calibration remains required.
