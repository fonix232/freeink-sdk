# PaperS3 EPDiy waveform strategy

This is a PaperS3-only waveform planner and native RMT gate-pulse transport helper, not an EPDiy library dependency. Data is adapted from [EPDiy ED047TC1](https://github.com/vroland/epdiy/blob/42c16127dc5d4bca15e830624f22666ad914575f/src/waveforms/epdiy_ED047TC1.h), revision `42c16127dc5d4bca15e830624f22666ad914575f`. Upstream firmware is LGPL-3.0; its license is retained in `LICENSE`. The compact tables and planner are LGPL-3.0-only. The RMT adapter is independently written against the installed ESP-IDF HAL.

The source tables have one temperature interval, 20–30°C. No colder/hotter calibration is claimed. Four application tones map to upstream levels 0, 5, 10, 15. Destination-major packed source tables are transposed into one 32-bit word per phase, representing all sixteen old/new transitions. There are no heap allocations. The three tables together occupy 544 bytes including explicit timing and neutral termination (8-byte Phase records).

`Mode::Text` preserves upstream GL16, 30 phases; `Mode::Grayscale` preserves GC16, 30 phases. GL16 leaves unchanged white untouched, but deliberately re-drives unchanged intermediate gray. Blanket masking of equal old/new tones changes the waveform and must not be applied. `Mode::DirectMono` preserves DU, five phases, valid for black/white destination images; use Text for arbitrary gray destinations. Each appends one neutral source scan for safe output release. This extra neutral scan is local and is not claimed to be part of EPDiy's source table.

## Timing and native integration

`Phase.gateHighTenthsUs` is a **gate pulse duration in 100 ns ticks**, not a delay between frames. The shortest pulse is 0.8 µs. Upstream I2S/RMT honors these durations in `output_i2s/render_i2s.c:36–62`; the S3 LCD renderer does not use `ctx->frame_time`. Copying only tables into a variable-duration GPIO scan does not reproduce the intended waveform.

`PaperS3GatePulse` supplies exact CKV timing from the PaperS3 40 MHz crystal divided by four. It uses one hardware RMT memory item and a zero terminator, directly accesses the ESP-IDF LL registers, polls raw completion without allocating an ISR, and times out in 1 ms. LCD DMA may run concurrently. It requires exclusive RMT group ownership; no Arduino RMT, LED-strip, or IDF RMT consumer may coexist. The native display renderer already serializes its own transactions; GatePulse is not a general concurrent RMT allocator.

1. Perform frame-start GPIO pulses with RMT detached.
2. `gate.acquire(PAPERS3_EP_CKV)` connects the pin matrix to RMT channel 0 and configures the clock.
3. After latching the prior source row and lowering LE, call `gate.start(phase.gateHighTenthsUs, phase.gateLowTenthsUs)` alongside starting LCD transfer of the next source row.
4. Wait for **both** LCD completion and `gate.wait()` before the next latch/row. An RMT failure must stop LCD/GDMA, power down EPD rails, invalidate the baseline and abort the refresh.
5. Preserve source/gate pipeline alignment: the last loaded row must also be latched and receive its specified gate pulse. Upstream I2S emits a final flush row (`render_i2s.c:260–263`). Do not replace that with an unbounded final GPIO high period.
6. `gate.release()` stops RMT, reconnects GPIO control with a low output, and disables peripheral/group/memory clocks. Frame-end GPIO pulses can then execute normally. Release on every failure too.

The helper compiles only for PaperS3 builds; it adds no RMT dependencies to other builds. It is not independently a full framebuffer driver. Root integration supplies DMA/framebuffer ownership and baseline recovery. For the existing erase/reset scans, an explicit 100-tick high/50-tick low pulse is available as `NEUTRAL` timing, but that is a local choice requiring optical verification.

`run(mode, transport)` preflights all timing capabilities before the first scan, stops on scan failure and invokes `abort()`. The transport's `scan(Phase)` must implement the sequence above; `supportsTiming(high,low)` must return false if timing cannot be represented.

## Validation

Run `python3 tests/run.py --upstream /path/to/pinned/epdiy`. ASan/UBSan exercises packed pixel expansion exhaustively, mode limits, unsupported timing preflight and transport failure. Mock register tests compile the actual RMT adapter and verify finite pulse encoding, exclusive ownership, and clock/matrix cleanup on timeout or hardware error. Source comparison checks all 1,040 transition pulses and 65 phase durations against the pinned original. The gate adapter also compiled with the installed Xtensa ESP32-S3 compiler and SDK headers. These checks do not establish physical contrast, gray accuracy or ghosting: verify repeated text page turns and all old/new gray transitions on the device, and inspect CKV/LE/SPH on a logic analyzer for exact waveform equivalence.

## PaperS3 presentation correction

The raw upstream tables remain unchanged and source-comparison tests still
cover all 1,040 transitions. `presentationPhase` composes them for PaperS3:
DU for monochrome destinations; five DU endpoint phases followed by GL16/GC16
phases masked to intermediate-gray destinations for antialiased presentation.
This addresses visibly retained black content: the raw GL16 black-to-white
sequence totals 55 us of gate drive, while DU supplies 500 us. It is a local
presentation policy, not an exact upstream waveform reproduction.

`PaperS3Driver` integrates this as `PaperS3Waveform::Epdiy`
(`FREEINK_PAPERS3_WAVEFORM=3`, or `setWaveform()` at runtime). It initializes
unknown glass with five white 100 us scans and one neutral scan, replacing the
three black/white reset cycles.

Hardware scan counting found the RMT prototype aborting partway through a
frame. The active integrated driver now uses CPU cycle-counted GPIO gate
pulses; the RMT helper and its mock tests are retained as reference only.
The current driver does not acquire an RMT channel.
