# Hardware Documentation — preenFM3

This document describes key hardware subsystems in the preenFM3 open-source synthesizer firmware: **audio output (CS4344 DACs)**, **display rendering (ILI9341 via SPI)**, **SD card storage**, **buttons/encoders**, and **MIDI**.

**MCU**: STM32H753VITx (H743/H753 family, Cortex-M7, LQFP100)  
**Clock**: 480 MHz system clock (PLL from 8 MHz HSE)  
**Project type**: STM32CubeIDE project  

---

## Table of Contents

- [STM32H743/H753 MCU Configuration](#stm32h743h753-mcu-configuration)
  - [MCU Variant](#mcu-variant)
  - [Clock Tree](#clock-tree)
  - [Enabled Peripherals](#enabled-peripherals)
  - [Overclock Status](#overclock-status)
- [Audio Output — CS4344 DACs](#audio-output--cs4344-dacs)
  - [Overview](#overview)
  - [SAI Configuration](#sai-configuration)
  - [DMA Setup](#dma-setup)
  - [Audio Sample Format & Rate](#audio-sample-format--rate)
  - [Sample Generation Pipeline](#sample-generation-pipeline)
  - [Double-Buffering Strategy](#double-buffering-strategy)
  - [Volume, Pan & Routing](#volume-pan--routing)
- [Display — ILI9341 Text & Graphics](#display--ili9341-text--graphics)
  - [Overview](#overview-1)
  - [SPI Communication](#spi-communication)
  - [Framebuffer Layout](#framebuffer-layout)
  - [Dirty Rectangle Update System](#dirty-rectangle-update-system)
  - [Text Rendering](#text-rendering)
  - [Graphics Primitives](#graphics-primitives)
  - [Oscilloscope & Algorithm Visualizer](#oscilloscope--algorithm-visualizer)
  - [Flush Pipeline](#flush-pipeline)
  - [UI Loop Architecture](#ui-loop-architecture)
- [FMDisplay3 — Screen Management & Action Queue](#fmdisplay3--screen-management--action-queue)
  - [Screen Classes](#screen-classes)
  - [refreshAllScreenByStep() State Machine](#refreshallscreenbystep-state-machine)
  - [Per-Screen Step Breakdown](#per-screen-step-breakdown)
  - [TFTAction Struct](#tftaction-struct)
  - [Action Types](#action-types)
  - [Methods That Push Actions](#methods-that-push-actions)
  - [Action Buffer Construction Pattern](#action-buffer-construction-pattern)
  - [Oscilloscope Background Queuing](#oscilloscope-background-queuing)
- [SD Card](#sd-card)
  - [Overview](#overview-2)
  - [SPI2 and DMA](#spi2-and-dma)
  - [Card Initialization](#card-initialization)
  - [Filesystem and Storage](#filesystem-and-storage)
  - [Cache Coherency Note](#cache-coherency-note)
- [Switches, Buttons & Encoders](#switches-buttons--encoders)
  - [HC165 Shift Register Chain](#hc165-shift-register-chain)
  - [Encoder Decoding](#encoder-decoding)
  - [Button Gestures](#button-gestures)
  - [MIDI Controller Mode](#midi-controller-mode)
- [MIDI](#midi)
  - [DIN MIDI (USART1)](#din-midi-usart1)
  - [USB MIDI](#usb-midi)
  - [MIDI Parser and Routing](#midi-parser-and-routing)
  - [MIDI Thru and Output](#midi-thru-and-output)
- [Peripheral Mapping Reference](#peripheral-mapping-reference)
- [Memory Map](#memory-map)

---

## STM32H743/H753 MCU Configuration

### MCU Variant

The STM32CubeIDE project metadata names the device **STM32H753VITx** (`firmware/preenfm3.ioc:122`), while code comments and the linker script also refer to the STM32H743 (`firmware/preenfm3_2MB.ld:7`). The H743 and H753 are the same Cortex-M7 family and are pin-compatible for this firmware; the H753 adds cryptographic peripherals that this firmware does not use.

| Parameter | Value |
|-----------|-------|
| Core | Arm Cortex-M7 |
| Package | LQFP100 (`VITx`) |
| Max CPU clock | 480 MHz |
| Supply configuration | LDO, VOS0 (`firmware/Src/main.c:214-217`) |
| Flash latency | 4 wait states (`main.c:255`) |
| HSE | 8 MHz (`firmware/Inc/stm32h7xx_hal_conf.h:99`, `preenfm3.ioc:386`) |
| HSI48 | 48 MHz, enabled for USB and RNG |

### Clock Tree

`SystemClock_Config()` is in `firmware/Src/main.c:206-289`.

| Clock | Source / formula | Frequency | Used by |
|-------|------------------|-----------|---------|
| HSE | External crystal | 8 MHz | PLL1/PLL2/PLL3 input |
| PLL1 VCO | `8 MHz / PLLM1(1) × PLLN1(120)` | 960 MHz | PLL1 outputs |
| SYSCLK | `PLL1 VCO / PLLP1(2)` | **480 MHz** | CPU, core peripherals |
| HCLK | `SYSCLK / 2` | 240 MHz | AHB, DMA, memories |
| APB1/2/3/4 | `HCLK / 2` | 120 MHz | APB peripherals |
| PLL2 VCO | `8 MHz / PLL2M(1) × PLL2N(40)` | 320 MHz | PLL2 outputs |
| PLL2P | `PLL2 VCO / PLL2P(5)` | 64 MHz | SPI1, SPI2 kernel clock |
| PLL2Q | `PLL2 VCO / PLL2Q(40)` | 8 MHz | USART1 kernel clock |
| PLL3 VCO | `8 MHz / PLL3M(1) × PLL3N(46)` | 368 MHz | PLL3 outputs |
| PLL3P | `PLL3 VCO / PLL3P(3)` | 122.67 MHz | SAI1/SAI2 kernel clock |
| HSI48 | Internal 48 MHz RC | 48 MHz | USB OTG FS, RNG |

The PLL2/PLL3 dividers are configured at `main.c:263-281`. SPI1/2/3 use PLL2P; SAI1/SAI2 use PLL3P; USART1 uses PLL2Q; USB and RNG use HSI48.

### Enabled Peripherals

| Peripheral | Instance | Role | Key pins | Init / notes |
|------------|----------|------|----------|--------------|
| DMA2D | DMA2D | TFT pixel composition | — | `MX_DMA2D_Init()` (`main.c:296`) |
| RNG | RNG | Hardware random source | — | `MX_RNG_Init()` (`main.c:343`), HSI48 |
| SAI1 | SAI1_Block_A/B | DAC audio outputs | PE2/PE3/PE4/PE5/PE6 | `MX_SAI1_Init()` (`main.c:370`) |
| SAI2 | SAI2_Block_A | DAC audio output | PD11 | `MX_SAI2_Init()` (`main.c:419`) |
| SPI1 | SPI1 | ILI9341 TFT | PA5/PA6/PA7, PD8/PD9/PD10 | `MX_SPI1_Init()` (`main.c:452`) |
| SPI2 | SPI2 | microSD card (SPI mode) | PA9/PB14/PB15, PE12 | `BSP_SPI_Init()` / `MX_SPI_Init()` (`stm32h7xx_nucleo_bus.c:80-100`) |
| USART1 | USART1 | DIN MIDI | PB6/PB7 | `MX_USART1_UART_Init()` (`main.c:567`) |
| USB OTG FS | USB | USB MIDI device | PA11/PA12 | `MX_USB_DEVICE_Init()` (`usb_device.c:68`) |
| TIM1 CH2 | TIM1 | TFT backlight PWM | PE11 | `MX_TIM1_Init()` (`main.c:660`) |
| DMA1 | DMA1 | SAI TX streams | — | `MX_DMA_Init()` (`main.c:619`) |
| DMA2 | DMA2 | SPI1/SPI2 streams | — | `MX_DMA_Init()` |
| GPIO | GPIOA/B/C/D/E/F/H | Board I/O, buttons, TFT/SD control | many | `preenfm3LibInitGpio()` (`preenfm3lib.c:29`) |
| FatFs | software | SD filesystem | — | `MX_FATFS_Init()` (`fatfs.c:30`) |
| Cortex-M7 caches/MPU | — | I-cache, D-cache, MPU regions | — | `SCB_EnableICache()`, `SCB_EnableDCache()`, `MPU_RegionConfig()` |

Not active in the normal build:

- **TIM3** is declared in `main.c` but its init call is inside the disabled `#ifdef UNDEFINED_PREENFM3_NEEDS_CONTROL_ON_GPIO_INIT` block (`main.c:134-150`), and no active `MX_TIM3_Init()` implementation is linked.
- **MDMA** has a handle declaration (`main.c:70`) but no initialization call.
- **SDMMC** is not used; the SD card is on SPI2.
- Internal ADC/DAC, I2C, CAN, Ethernet, LTDC and USB audio class are not enabled.

### Overclock Status

The CPU is **not overclocked**:

```text
8 MHz HSE / PLLM1(1) × PLLN1(120) = 960 MHz VCO
960 MHz / PLLP1(2) = 480 MHz SYSCLK
```

480 MHz is the maximum specified SYSCLK for the STM32H743/H753. The `OVERCLOCK` preprocessor symbol only changes the firmware version string in `firmware/Src/hardware/Menu.cpp:22-25`; it does not change any PLL setting.

The actual overclock in the design is the **ILI9341 SPI bus**: PLL2P is 64 MHz and SPI1 uses a `/2` prescaler, giving approximately **32 MHz SCK**, above the panel's nominal 10 MHz write rating. See `doc/improvements.md` for that issue.

---

## Audio Output — CS4344 DACs

### Overview

The preenFM3 uses **three CS4344 stereo DAC chips**, providing **six analog outputs** total. Each DAC is driven by one SAI (Serial Audio Interface) block on the STM32H753. The audio path uses I2S protocol over DMA with a block-based, double-buffered architecture.

### SAI Configuration

Three SAI blocks are configured:

| Block | Mode | Pins | Sync |
|-------|------|------|------|
| **SAI1_Block_A** | Master TX | PE2=MCLK, PE4=FS, PE5=SCK, PE6=SD | — (clock source) |
| **SAI1_Block_B** | Slave TX | PE3=SD | Synced to Block A |
| **SAI2_Block_A** | Slave TX | PD11=SD | Synced externally to SAI1 |

- **Protocol**: Philips standard I2S (`SAI_I2S_STANDARD`)
- **Data size**: 32-bit per channel
- **Audio mode**: Stereo (`SAI_STEREOMODE`)
- **No companding**: `SAI_NOCOMPANDING`

Clock source is PLL3 (N=46, P=3), yielding a SAI clock of ~122.67 MHz.

### DMA Setup

Each SAI block has a dedicated DMA stream for memory-to-peripheral transfers:

| SAI Block | DMA Stream | IRQ Priority |
|-----------|-----------|--------------|
| SAI1_Block_A | DMA1_Stream0 | 3 / 0 |
| SAI1_Block_B | DMA1_Stream1 | 3 / 0 |
| SAI2_Block_A | DMA1_Stream2 | 3 / 0 |

All streams use:
- Circular mode
- Word (32-bit) alignment
- Very high priority
- FIFO threshold at 1/4 full

### Audio Sample Format & Rate

| Parameter | Value |
|-----------|-------|
| Sample rate | 48 kHz nominal (actual: ~47.916 kHz, error −0.17%) |
| Data format | 32-bit integers (24-bit signed, left-justified) |
| Block size | 32 samples per channel per callback |
| DMA transfer size | 64 words (32 stereo pairs) |

Synthesis internally uses `float` (32-bit IEEE 754). Samples are converted to `int32_t` with a left-shift of 8 bits before DAC output. Clipping is detected against threshold `0x7FFFFF`.

### Sample Generation Pipeline

1. **Voice level**: Each voice generates stereo float samples into `sampleBlock[BLOCK_SIZE * 2]`.
2. **Timbre mixing**: Voices mix into timbre-level buffers.
3. **Synth::buildNewSampleBlock()**: Main processing function that:
   - Processes all 6 timbres
   - Applies smoothed volume and pan
   - Runs per-timbre compression
   - Routes each timbre to one of 3 output buffers (waveform1/2/3) based on instrument routing settings
   - Detects clipping and limits output
   - Converts floats to left-justified 24-bit int32_t

Output buffers are declared in `preenfm3.cpp`:

```cpp
RAM_D2_SECTION int32_t waveform1[64 * 2];  // DAC output 1 (stereo)
RAM_D2_SECTION int32_t waveform2[64 * 2];  // DAC output 2 (stereo)
RAM_D2_SECTION int32_t waveform3[64 * 2];  // DAC output 3 (stereo)
```

They reside in RAM_D2 (@0x30000000), which is cacheable but non-bufferable per MPU configuration.

### Double-Buffering Strategy

Each 64-sample buffer is split into two halves of 32 samples. Circular DMA continuously reads from the buffer while the CPU fills the alternate half in the SAI DMA completion callbacks:

- **Half-complete callback** (~667 µs interval): Fills first half
- **Complete callback** (~1333 µs interval): Fills second half + processes MIDI input

Both callbacks call `synth.buildNewSampleBlock()` and record oscilloscope samples.

### Volume, Pan & Routing

- **Per-timbre volume**: Smoothed exponentially (`old * 0.9f + new * 0.1f`), then applied as gain with a 0.2f headroom factor.
- **Per-timbre pan**: Smoothed (`old * 0.95f + new * 0.05f`), range 0–63 mapped to −1.0…+1.0.
- **Output routing**: Each timbre routes to specific DAC outputs:
  - `out = 0`: Both channels
  - `out = 1`: Left only
  - `out = 2`: Right only
  - Same pattern repeats for DAC 2 and DAC 3

---

## Display — ILI9341 Text & Graphics

### Overview

The display is an **ILI9341 controller** driving a **240×320 RGB565 TFT** panel over SPI. The firmware implements a custom lightweight display system (no LVGL). All drawing is accelerated by the STM32H7's **DMA2D** (2D graphics accelerator) and **SPI DMA**.

### SPI Communication

| Parameter | Value |
|-----------|-------|
| Peripheral | SPI1 |
| Pins | PA5=SCK, PA6=MISO, PA7=MOSI |
| Baud rate prescaler | `/2` (`firmware/Src/main.c:485`) |
| SPI kernel clock | PLL2P = 64 MHz (`firmware/Src/main.c:263-281`); SCK ≈ 32 MHz |
| DMA | DMA2_Stream0 (high priority, MEM->PERIPH, normal mode) |
| Color format | RGB565 (16-bit pixels) |
| Orientation | Portrait (240W × 320H), MADCTL flags: MX, BGR |

TFT control pins: CS=PD10, DC=PD8, RESET=PD9 (`lib/Inc/preenfm3_pins.h:45-50`). The display driver includes `preenfm3_pins.h`; the CubeMX-generated aliases in `firmware/Inc/main.h:72-77` use different pins and are not used by the display driver.

Backlight: TIM1 Channel 2 on PE11 (`firmware/Src/stm32h7xx_hal_msp_pfm3.c:676-686`). `LED_CONTROL` on PB1 is a separate status LED.

### Framebuffer Layout

All display buffers are in RAM_D1 (`.ram_d1`), not DTCM:

| Buffer | Size | Purpose |
|--------|------|---------|
| `tftMemory[240 * 320]` | 76,800 `uint16_t` = 153,600 B | RGB565 framebuffer sent to the TFT |
| `tftForeground[60000]` | 60,000 B | A8 masks: font glyphs, oscilloscope, algorithm view |
| `tftBackground[42860 * 2]` | 85,720 `uint16_t` = 171,440 B | RGB565 backgrounds and pre-filled character blocks |

The foreground buffer is subdivided during `TftDisplay::init()` (`lib/Src/TftDisplay.cpp:143-230`):

| Buffer | Size | Purpose |
|--------|------|---------|
| `fgBigChars` | 22,968 B | 116 glyphs × 18 rows × 11 columns, A8 (`NUMBER_OF_CHARS = 95 + 16 + 1 + 4`, `TftDisplay.cpp:27`) |
| `fgSmallChars` | 6,720 B | 96 glyphs × 10 rows × 7 columns, A8 |
| `fgSegmentDigit` | 2,160 B | 12 segment digits × 9 rows × 20 bytes |
| `fgOscillo` | 16,160 B | Oscilloscope foreground, 160 × 101 A8 |
| `fgAlgo` | 8,000 B | Algorithm visualizer foreground, 80 × 100 A8 |

Background buffers (RGB565) include `bgColorChar[color]` pre-filled character blocks, `bgOscillo` (160 × 100), and `bgAlgo` (80 × 100).

### Dirty Rectangle Update System

The screen is divided into **8 horizontal parts** tracked by the `tftDirtyBits` bitmap (`lib/Src/TftDisplay.cpp:74-92`):

| Part | Bit | Y range | Height | Constant |
|------|-----|---------|--------|----------|
| Header | 0 | 0–40 | 40 | `TFT_PART_HEADER` |
| Values | 1 | 40–80 | 40 | `TFT_PART_VALUES` |
| Values2 | 2 | 80–120 | 40 | `TFT_PART_VALUES2` |
| Values3 | 3 | 120–160 | 40 | `TFT_PART_VALUES3` |
| Oscillo | 4 | 160–200 | 40 | `TFT_PART_OSCILLO` |
| Oscillo2 | 5 | 200–240 | 40 | `TFT_PART_OSCILLO2` |
| Oscillo3 | 6 | 240–270 | 30 | `TFT_PART_OSCILLO3` |
| Buttons | 7 | 270–320 | 50 | `TFT_PART_BUTTONS` |

`setDirtyArea(y, height)` (`TftDisplay.cpp:278-313`) sets bits based on Y overlap. `TFT_PART_VALUES_BITS` (`0b00001110`) covers parts 1–3; `TFT_PART_OSCILLO_BITS` (`0b01110000`) covers parts 4–6. `pushToTft()` sends at most one dirty part per call, rotating through parts via the `part` index.

### Text Rendering

Text uses a **composite approach**: background blocks are pre-filled in RGB565, and foreground alpha masks (A8) are blended on top via DMA2D.

| Font | Size | Usage |
|------|------|-------|
| `Font_11x18` | 11×18 px | Main text; cursor cell is 11×20 |
| `Font_7x10` | 7×10 px | Status bar / labels |

Rendering pipeline:
1. UI code calls `print()` / `printSmallChar()`, which enqueue one `TFTAction` per character.
2. `TFT_DISPLAY_ONE_CHAR` uses `fgBigChars + glyph * 18 * 11`, `bgColorChar[bgColor]`, and the current text color. `TFT_DISPLAY_ONE_SMALL_CHAR` uses `fgSmallChars + glyph * 10 * 7`.
3. `TftDisplay::tic()` runs the DMA2D `M2M_BLEND` operation when DMA2D is free, then marks the affected part dirty.

`print(const char*)`, `print(int)`, and `print(float)` are wrappers that decompose the value into character actions. `setCursor(x, y)` uses the 11×20 text cell; `setCursorInPixel(x, y)` positions directly in pixels.

### Graphics Primitives

| Primitive | Implementation |
|-----------|---------------|
| Fill area / clear screen | DMA2D Register-to-Memory (`R2M`) with solid color |
| Rectangles | DMA2D blend or direct pixel writes |
| Lines (algo viz) | Bresenham algorithm writing directly to buffers |
| Pixels | Direct array access via `SETPIXEL` macro |
| Buttons | Composite of background + foreground + text actions |

### Oscilloscope & Algorithm Visualizer

**Oscilloscope** (`oscilloRrefresh()`):
- `oscilloRecord32Samples()` is called from the SAI DMA callbacks and stores samples in `oscilloFullPeriodSampl[]`.
- `preenfm3Loop()` calls `oscilloRrefresh()` every 125 ms (8 Hz) in editor mode.
- The waveform is drawn as filled vertical lines between adjacent samples into `fgOscillo` (A8).
- `TFT_DRAW_OSCILLO` is a 2-step action: DMA2D blends `fgOscillo` over `bgOscillo` into `tftMemory`, then clears `fgOscillo` with an R2M fill and marks the oscillo parts dirty.
- `oscilloForceNextDisplay()` forces a redraw after a pause or mode change.

**Algorithm visualizer** (`TftAlgo`):
- `TFT_DRAW_ALGO` is a multi-step action: clear `bgAlgo`, draw the algorithm with `TftAlgo::drawAlgo()`, optionally clear `fgAlgo`, then blend `fgAlgo` over `bgAlgo` into `tftMemory`.
- Custom Bresenham line drawing into RGB565 and A8 buffers.
- Numbers rendered as scaled (2×) 5×5 digit patterns; operators drawn as boxes.

### Flush Pipeline

`pushToTft()` (`lib/Src/TftDisplay.cpp:641-675`) works as follows:

1. Returns immediately if `tftDirtyBits == 0` or `tftRefreshing == false`.
2. Scans from the current `part` index and selects the next dirty part.
3. Calls `ILI9341_SetAddressWindow(y, y + height - 1)`.
4. Starts `HAL_SPI_Transmit_DMA()` for `height * 240 * 2` bytes from `tftMemory`.
5. On successful start, clears that part's dirty bit and sets `pushToTftInProgress`.

`HAL_SPI_TxCpltCallback` (`firmware/Src/preenfm3.cpp:358-363`) calls `pushToTftFinished()`, which deselects CS and clears `pushToTftInProgress`. `TFT_RESTART_REFRESH` sets `tftRefreshing = true` and `part = 0`; `TFT_PAUSE_REFRESH` sets `tftRefreshing = false`.

### UI Loop Architecture

```
SysTick_Handler()       (1 kHz)
└── preenfm3Tic()
    ├── tftCpt++
    ├── encoders.checkStatus() every 2 ticks (500 Hz)
    ├── sequencer.ticMillis()
    ├── tft.tic()       every tick
    │   ├── pushToTft() at most every 20 ms; if a part is pushed, return
    │   ├── pop one action when currentAction is empty
    │   ├── process at most one action while DMA2D is free
    │   └── additionalActions() for firmware-specific actions
    └── tempoClick / LED timing every 256 ticks

preenfm3Loop()          (main while(1), no fixed rate)
├── MIDI async actions
├── Encoder/button actions
├── Screen refresh (fmDisplay3.refreshAllScreenByStep()) while queue < 100
├── Oscilloscope refresh (8 Hz)
└── Clipping indicator / CPU usage
```

Design principles:
- **Non-blocking**: All operations use action queues and multi-step processing.
- **Hardware-accelerated**: DMA2D for pixel composition, SPI DMA for transfer.
- **Chunked updates**: Screen divided into 8 parts, one part per `pushToTft()` call.
- **Paced flush**: `pushToTft()` is attempted at most every 20 ms, not every tick.
- **Decoupled audio/display**: Audio runs at 48 kHz; display updates are asynchronous.

---

## FMDisplay3 — Screen Management & Action Queue

The `FMDisplay3` class coordinates four top-level screens (Mixer, Editor, Menu, Sequencer) and manages incremental screen rendering via a step-based state machine that pushes drawing actions into the `TftDisplay` action queue. It holds a `FirmwareTftDisplay *tft_`, not a plain `TftDisplay`; `FirmwareTftDisplay` extends the base class with oscilloscope background actions handled in `additionalActions()`. This section covers how screens are structured, how `refreshAllScreenByStep()` works, and how the action buffer is constructed.

### Screen Classes

Four screen classes, each responsible for one top-level display mode:

| Class | File | Purpose |
|-------|------|---------|
| `FMDisplayMixer` | `firmware/Src/hardware/FMDisplayMixer.h` | Mixer screen — instrument levels, pan, FX sends |
| `FMDisplayEditor` | `firmware/Src/hardware/FMDisplayEditor.h` | Editor screen — operator params, matrix, LFO, modulators |
| `FMDisplayMenu` | `firmware/Src/hardware/FMDisplayMenu.h` | Menu screen — file browser, presets, settings |
| `FMDisplaySequencer` | `firmware/Src/hardware/FMDisplaySequencer.h` | Sequencer screen — step sequencer and pattern playback |

These are owned by the central coordinator:

```cpp
// firmware/Src/hardware/FMDisplay3.h:32
class FMDisplay3 : public SynthParamListener, public SynthMenuListener,
                   public SynthStateAware, public VisualInfo {
    FMDisplayMixer *displayMixer_;       // line 100
    FMDisplayMenu *displayMenu_;         // line 101
    FMDisplayEditor *displayEditor_;     // line 102
    FMDisplaySequencer *displaySequencer_; // line 103
};
```

The active screen is determined by `synthMode` in the full state:
- `SYNTH_MODE_MIXER` → Mixer
- `SYNTH_MODE_EDIT_PFM3` → Editor
- `SYNTH_MODE_MENU` → Menu
- `SYNTH_MODE_SEQUENCER` → Sequencer

`FMDisplay3` also implements `VisualInfo`. `tempoClick()` decrements the per-timbre note-on counters and forwards to the active screen's `tempoClick()` for level-meter decay. `midiClock()` and `noteOn()` update small status indicators.

### refreshAllScreenByStep() State Machine

**File**: `firmware/Src/hardware/FMDisplay3.cpp`, lines 76–98

```cpp
void FMDisplay3::refreshAllScreenByStep() {
    switch (this->synthState_->fullState.synthMode) {
        case SYNTH_MODE_MIXER:
            displayMixer_->refreshMixerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
            break;
        case SYNTH_MODE_EDIT_PFM3:
            displayEditor_->refreshEditorByStep(refreshStatus_, endRefreshStatus_);
            break;
        case SYNTH_MODE_MENU:
            displayMenu_->refreshMenuByStep(currentTimbre_, refreshStatus_);
            break;
        case SYNTH_MODE_SEQUENCER:
            displaySequencer_->refreshSequencerByStep(currentTimbre_, refreshStatus_, endRefreshStatus_);
            break;
    }

    if (refreshStatus_ > 0) {
        refreshStatus_--;
    }
    if (refreshStatus_ == 0) {
        tft_->restartRefreshTft();
    }
}
```

**State fields** (`FMDisplay3.h`, lines 91–92):
```cpp
int refreshStatus_;    // Current countdown value (acts as the step number)
int endRefreshStatus_; // Optional lower step at which to stop a partial refresh
```

**How it works:**
1. A full screen change sets `refreshStatus_` to 21 (`newSynthMode()`), or 20 at startup (`preenfm3Init()`). Partial refreshes set smaller ranges, e.g. `newTimbre()` sets `refreshStatus_ = 19`, `endRefreshStatus_ = 14` for the mixer.
2. Each call to `refreshAllScreenByStep()` invokes the active screen's `refreshXxxByStep()` once.
3. The screen method uses `switch(refreshStatus_)` to draw one step. Some steps push many actions; some are no-ops.
4. After the screen method returns, `refreshStatus_` is decremented by one.
5. For a partial refresh (`endRefreshStatus_ > 0`), the screen method resets both counters to 0 when the countdown reaches that step. For a full refresh (`endRefreshStatus_ == 0`), `refreshAllScreenByStep()` calls `tft_->restartRefreshTft()` when the countdown hits 0.
6. `needRefresh()` returns `refreshStatus_ != 0`. The menu screen does not use `endRefreshStatus_`; it stops when the countdown reaches 0.

**Call sites** (`firmware/Src/preenfm3.cpp`):
```cpp
// Main loop path (lines 122–124, 205–207):
while (fmDisplay3.needRefresh()) {
    fmDisplay3.refreshAllScreenByStep();
}

// With action buffer throttling:
if (fmDisplay3.needRefresh() && tft.getNumberOfPendingActions() < 100) {
    while (fmDisplay3.needRefresh() && tft.getNumberOfPendingActions() < 100) {
        fmDisplay3.refreshAllScreenByStep();
    }
}
```

The action buffer throttle (`getNumberOfPendingActions() < 100`) prevents queue overflow when the display DMA pipeline is still processing previous actions.

### Per-Screen Step Breakdown

Each screen divides its redraw into numbered steps (cases), pushing actions into the queue per step. A full refresh is usually started at 21, so step 21 is often a no-op; drawing starts at 20. Steps not listed below have no case and are no-ops.

#### Mixer (`FMDisplayMixer::refreshMixerByStep`, lines 546–645): Steps 20→1

| Step(s) | Content Drawn |
|---------|---------------|
| 20 | Header bar ("MIX" + mix name) |
| 19–14 | Timbre names (one per step) |
| 12–7 | Mixer values for each timbre |
| 6 | Selected parameter title |
| 5–1 | Bottom row buttons |

#### Editor (`FMDisplayEditor::refreshEditorByStep`, lines 3269–3491): Steps 21→1

| Step(s) | Content Drawn |
|---------|---------------|
| 21 | No-op countdown entry |
| 20 | `mainPage == -1`: pause refresh, clear screen, display preset, force oscilloscope refresh; otherwise page header/black fill |
| 19–14 | Main page buttons (`mainPage == -1`) |
| 13 | "E d i t o r" label (`mainPage == -1`) |
| 10 | Draw algorithm diagram (`mainPage == -1`) |
| 18–14 | Page buttons |
| 12–7 | Parameter names |
| 6–1 | Parameter values |

#### Menu (`FMDisplayMenu::refreshMenuByStep`, lines 35–160): Steps 20, 6→1

| Step(s) | Content Drawn |
|---------|---------------|
| 20 | Header bar ("M E N U") |
| 6–1 | Button labels based on current menu type |

#### Sequencer (`FMDisplaySequencer::refreshSequencerByStep`, lines 55–229): Steps 20→1

| Step(s) | Content Drawn |
|---------|---------------|
| 20 | Header bar ("SEQ" + sequence name) |
| 19 | Tempo info |
| 18 | Memory percentage |
| 17 | Beat display |
| 16 | Clear sequence area |
| 15 | Activated indicators |
| 14–9 | Instrument rows |
| 7 | Mode label |
| 6–1 | Bottom buttons |

### TFTAction Struct

**File**: `lib/Inc/TftDisplay.h`, lines 104–111

```cpp
struct TFTAction {
    uint8_t actionType :4;   // Which action (0x0–0xF)
    uint8_t param1;
    uint16_t param2;
    uint8_t param3;
    uint16_t param4;
    uint8_t param5;
};
```

The fields are **action-specific**, not a universal coordinate tuple:

| Action | `param1` | `param2` | `param3` | `param4` | `param5` |
|--------|----------|----------|----------|----------|----------|
| `TFT_DISPLAY_ONE_CHAR` | x pixel | y pixel | glyph index | text color | background color |
| `TFT_DRAW_FILL_AREA` | x | y | width | height | color |
| `TFT_DRAW_ALGO` | algo number | — | step | — | — |
| `TFT_DRAW_OSCILLO` | saturation flag | — | step | — | — |
| `TFT_DRAW_FILL_TFT` | — | — | part index | — | color |
| `TFT_WAITCYCLE` | — | countdown | — | — | — |
| `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM` | waveform selector | — | step | — | — |

The action queue is `RingBuffer<TFTAction, TFTACTION_BUFFER_SIZE>` with `TFTACTION_BUFFER_SIZE = 128` (`TftDisplay.h:29`). `RingBuffer::insert()` does not check `isFull()`; callers rely on the `< 100` pending-action throttle to avoid overflow.

### Action Types

Standard action types (`TftDisplay.h`, lines 40–57):

| Value | Type | Description |
|-------|------|-------------|
| 0 | `TFT_NOP` | No operation |
| 1 | `TFT_DISPLAY_ONE_CHAR` | Display one big character (11×18) |
| 2 | `TFT_DISPLAY_ONE_SMALL_CHAR` | Display one small character (7×10) |
| 3 | `TFT_DRAW_FILL_TFT` | Fill entire TFT area (8-part multi-step) |
| 4 | `TFT_DRAW_OSCILLO` | Draw oscilloscope waveform (2-step) |
| 5 | `TFT_DRAW_ALGO` | Draw algorithm diagram (4-step) |
| 6 | `TFT_HIGHLIGHT_ALGO_OPERATOR` | Highlight an operator in algo |
| 7 | `TFT_ERASE_HIGHLIGHT_ALGO_OPERATOR` | Erase operator highlight |
| 8 | `TFT_HIGHTLIGHT_ALGO_IM` | Highlight an IM (index modulation) path |
| 9 | `TFT_ERASE_HIGHTLIGHT_ALGO_IM` | Erase IM highlight |
| 10 | `TFT_CLEAR_ALGO_FG` | Clear algo foreground buffer |
| 11 | `TFT_DRAW_FILL_AREA` | Fill rectangular area with color |
| 12 | `TFT_RESTART_REFRESH` | Restart screen refresh from top |
| 13 | `TFT_PAUSE_REFRESH` | Pause screen refresh |
| 14 | `TFT_WAITCYCLE` | Wait `param2` ticks before continuing; also aliased by `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM` |

Firmware-specific action (`firmware/Src/hardware/FirmwareTftDisplay.h:36-38`):

| Value | Type | Description |
|-------|------|-------------|
| 14 | `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM` | Alias of `TFT_STANDARD_LAST_ACTION` / `TFT_WAITCYCLE`; handled by `FirmwareTftDisplay::additionalActions()`, not by the standard switch |

The values below are **not action types**. They are selectors stored in `TFTAction.param1` when the action type is `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM`:

| `param1` | Selector | Meaning |
|----------|----------|---------|
| 0–13 | `TFT_NUMBER_OF_WAVEFORM_EXT` range | Operator waveform index |
| 15 | `TFT_DRAW_ENVELOPPE` | Draw envelope shape in the oscilloscope background |
| 16 | `TFT_DRAW_LFO` | Draw LFO shape in the oscilloscope background |

`TFTAction.actionType` is a 4-bit field, so it can hold values 0–15. This is why the oscilloscope background action reuses action type 14 and stores the waveform selector in `param1`.

### Methods That Push Actions

Every drawing method inserts a `TFTAction` into the ring buffer. Below are all methods organized by their source file.

#### TftDisplay core methods (`lib/Src/TftDisplay.cpp`)

| Method | Line | Action Type Inserted |
|--------|------|---------------------|
| `print(char c)` | 894 | `TFT_DISPLAY_ONE_CHAR` |
| `printSmallChar(char c)` | 910 | `TFT_DISPLAY_ONE_SMALL_CHAR` |
| `fillArea(x, y, w, h, color)` | 1010 | `TFT_DRAW_FILL_AREA` |
| `clear()` | 726 | `TFT_DRAW_FILL_TFT` |
| `drawAlgo(int algo)` | 1044 | `TFT_DRAW_ALGO` |
| `highlightOperator(int op)` | 1058 | `TFT_HIGHLIGHT_ALGO_OPERATOR` |
| `eraseHighlightOperator(int op)` | 1066 | `TFT_ERASE_HIGHLIGHT_ALGO_OPERATOR` |
| `highlightIM(from, to)` | 1074 | `TFT_HIGHTLIGHT_ALGO_IM` |
| `eraseHighlightIM(from, to)` | 1084 | `TFT_ERASE_HIGHTLIGHT_ALGO_IM` |
| `clearAlgoFG()` | 1052 | `TFT_CLEAR_ALGO_FG` |
| `oscilloRrefresh()` | 926 | `TFT_DRAW_OSCILLO` |
| `restartRefreshTft()` | 1094 | `TFT_RESTART_REFRESH` |
| `pauseRefresh()` | 1100 | `TFT_PAUSE_REFRESH` |
| `waitCycle(uint16_t n)` | 1107 | `TFT_WAITCYCLE` |
| `clearMixerValues()` | 697 | `TFT_DRAW_FILL_AREA` |
| `clearMixerLabels()` | 714 | `TFT_DRAW_FILL_AREA` |
| `drawButton(label, x, y, state)` | 1116 | `fillArea` + `print` (composite) |
| `drawSimpleButton(label, x, y, color)` | 1180 | `fillArea` + `print` (composite) |
| `drawButtonBiState(label, x, y, state, onColor)` | 1201 | `fillArea` + `print` (composite) |
| `drawSimpleBorderButton(label, x, y, color)` | 1237 | `fillArea` × 2 + `print` (composite) |
| `drawLevelMetter(x, y, width, height, pixelPerDb, volume, isComp, gr)` | 1304 | Multiple `fillArea` calls |
| `printSmallChars(const char*)` | 920 | One `TFT_DISPLAY_ONE_SMALL_CHAR` per character |
| `printValueWithSpace(int)` | 1258 | `print()` character actions with padding |
| `printFloatWithSpace(float)` | 1274 | `print()` character actions with padding |
| `clearActions()` | 131 | Clears the pending action queue (no action inserted) |

#### FirmwareTftDisplay extensions (`firmware/Src/hardware/FirmwareTftDisplay.cpp`)

| Method | Line | Description |
|--------|------|-------------|
| `oscilloBgActionOperatorShape(int op)` | 150 | Queue oscilloscope background: operator waveform |
| `oscilloBgActionEnvelope()` | 184 | Queue oscilloscope background: envelope shape |
| `oscilloBgActionLfo()` | 166 | Queue oscilloscope background: LFO shape |
| `oscilloBgActionClear()` | 138 | Clear oscilloscope background |
| `additionalActions()` | 63 | Override hook for firmware-specific actions |

#### Call Sites by Screen Class

Every screen class calls these methods through its `tft_` pointer:

| Screen | Methods Called |
|--------|---------------|
| `FMDisplayMixer` | `tft_->print()`, `tft_->fillArea()`, `tft_->drawButton()`, `tft_->drawLevelMetter()`, `tft_->clear()`, `tft_->pauseRefresh()`, `tft_->restartRefreshTft()` |
| `FMDisplayEditor` | `tft_->print()`, `tft_->fillArea()`, `tft_->drawButton()`, `tft_->drawButtonBiState()`, `tft_->drawAlgo()`, `tft_->clearAlgoFG()`, `tft_->highlightOperator()`, `tft_->eraseHighlightOperator()`, `tft_->highlightIM()`, `tft_->eraseHighlightIM()`, `tft_->oscilloForceNextDisplay()`, plus oscillo background methods |
| `FMDisplaySequencer` | `tft_->print()`, `tft_->fillArea()`, `tft_->drawSimpleButton()`, `tft_->clear()` |
| `FMDisplayMenu` | `tft_->print()`, `tft_->fillArea()`, `tft_->drawButton()`, `tft_->drawSimpleButton()`, `tft_->drawSimpleBorderButton()`, `tft_->clear()` |

### Action Buffer Construction Pattern

The action construction follows a consistent pattern across all screens:

1. **A step function receives `refreshStatus_`** as a switch case discriminator
2. **Inside each case**, the screen class calls `tft_` methods that internally:
   - Allocate a `TFTAction` slot from the ring buffer via `tftActions.insert()`
   - Populate the action struct fields (`actionType`, `param1`–`param5`)
   - Set the dirty area bits via `setDirtyArea(y, height)` where applicable
3. **Composite actions** (e.g., `drawButton`) push multiple actions sequentially — typically a `fillArea` for the button background followed by one or more `print` actions for the label
4. **Multi-step actions** (e.g., `TFT_DRAW_FILL_TFT`, `TFT_DRAW_ALGO`) use `param3` as an internal step counter that advances each time `tic()` processes the action

The ring buffer capacity is 128. When `getNumberOfPendingActions()` approaches this limit, `refreshAllScreenByStep()` back-pressure throttles to prevent overflow.

**How `tic()` drains the queue** (`lib/Src/TftDisplay.cpp:317-635`):

1. Every call first tries `pushToTft()` at most once per 20 ms. If a part was pushed, `tic()` returns early.
2. If `currentAction.actionType == 0` and the queue is non-empty, one `TFTAction` is removed from the queue and becomes `currentAction`.
3. The `switch(currentAction.actionType)` executes at most one step. Multi-step actions leave `actionType` set and advance their `param3` step counter; single-step actions clear `actionType` to 0.
4. `additionalActions()` runs after the switch. `FirmwareTftDisplay` uses it for `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM`.

This means the display engine is deliberately cooperative: one part flush or one action step per tick, never a full-screen synchronous redraw.

### Oscilloscope Background Queuing

The editor screen can overlay reference waveforms behind the live oscilloscope trace. These are queued separately from the main screen redraw:

**File**: `firmware/Src/hardware/FirmwareTftDisplay.cpp`

| Method | Line | What It Queues |
|--------|------|---------------|
| `oscilloBgActionOperatorShape(int op)` | 150 | `TFT_DRAW_OSCILLO_BACKGROUND_WAVEFORM` with `param1 = op` |
| `oscilloBgActionLfo()` | 166 | Same action with `param1 = TFT_DRAW_LFO` (16) |
| `oscilloBgActionEnvelope()` | 184 | Same action with `param1 = TFT_DRAW_ENVELOPPE` (15) |
| `oscilloBgActionClear()` | 138 | Same action with `param1 = 255`, which clears the background layer |

These are invoked from the editor screen (e.g., `FMDisplayEditor::refreshOscillatorOperatorShape()`, `firmware/Src/hardware/FMDisplayEditor.cpp:4293`) when the user switches to pages that benefit from visual reference shapes. The background is drawn into `bgOscillo` and composited beneath the live waveform during `oscilloRrefresh()`.

`FirmwareTftDisplay::clearActions()` also resets the `envInQueue`, `lfoInQueue`, and `operatorInQueue` counters so stale background requests do not survive a screen clear.

---

## SD Card

### Overview

The microSD card is used in **SPI mode**, not SDMMC. It is connected to SPI2 and accessed through a FatFs disk I/O layer.

`preenfm3Init()` calls `preenfm3LibInitSD()` (`firmware/Src/preenfm3.cpp:96`), which:

1. Calls `ADAFRUIT_802_SD_Init(0)` (`lib/Src/preenfm3lib.c:145-151`).
2. Mounts the FatFs volume with `f_mount(&USERFatFS, USERPath, 1)`.

`USERFatFS` and the SD/FatFS buffers live in `.ram_d2b` (`lib/Src/fatfs.c:24`). The `Storage sdCard` object (`preenfm3.cpp:59`) wraps the preset, mixer, sequence, configuration and user-waveform files. On startup, `preenfm3Init()` loads the configuration, default mixer, default sequence, user waveforms and user envelopes (`preenfm3.cpp:468-472`).

The card contains:

- Presets and patch banks (`PatchBank`)
- Mixer banks (`MixerBank`)
- Sequences (`SequenceBank`)
- Configuration (`ConfigurationFile`)
- User waveforms and envelopes
- Optional PPM screenshots (`PPMImage.cpp`)

### SPI2 and DMA

| Parameter | Value |
|-----------|-------|
| Peripheral | SPI2 |
| Pins | PA9=SCK, PB14=MISO, PB15=MOSI (`firmware/Src/stm32h7xx_hal_msp_pfm3.c:240-255`) |
| Chip select | PE12, active low, software-controlled (`lib/Inc/preenfm3_pins.h:41-42`, `lib/Inc/adafruit_802_conf.h:41-42`) |
| SPI mode | Master, 8-bit, CPOL high, CPHA 2 edge, software NSS (`lib/Src/stm32h7xx_nucleo_bus.c:129-145`) |
| Target bitrate | 15 MHz (`BUS_SPI_BAUDRATE`, `lib/Inc/stm32h7xx_nucleo_bus.h:56`); prescaler computed from HCLK (`stm32h7xx_nucleo_bus.c:322-345`) |
| DMA RX | DMA2_Stream1, normal mode, byte-aligned, high priority (`stm32h7xx_hal_msp_pfm3.c:257-275`) |
| DMA TX | DMA2_Stream2, normal mode, byte-aligned, high priority (`stm32h7xx_hal_msp_pfm3.c:278-295`) |

`USE_DMA_FOR_SD_ACCESS` is `true` in `firmware/Src/main.c:91`, so `MX_FATFS_Init(USE_DMA_FOR_SD_ACCESS)` (`main.c:159`) links the DMA disk I/O driver (`fatfs.c:30-37`). `SD_read_DMA()` and `SD_write_DMA()` call `PFM3_SD_ReadBlocks()` / `PFM3_SD_WriteBlocks()` with `useDMA = true` (`lib/Src/sd_diskio.c:144-196`).

The block layer uses 512-byte sectors and issues CMD17 (read single block) / CMD24 (write single block) per sector. SDHC cards use block addresses; SDSC cards use byte addresses (`lib/Src/adafruit_802_sd.c:390-510`). DMA transfers go through:

`SD_IO_WriteReadData_DMA()` → `BSP_SPI_SendRecv_DMA()` → `HAL_SPI_TransmitReceive_DMA(&sd_spi2, ...)` (`adafruit_802_sd.c:1453-1456`, `stm32h7xx_nucleo_bus.c:215-221`).

`spi2TransferComplete()` sets the `spi2TransferState` flag from the SPI2 completion/error callbacks (`adafruit_802_sd.c:249-253`, `firmware/Src/stm32h7xx_it.c:376-395`). `SD_read_DMA()` / `SD_write_DMA()` then poll `ADAFRUIT_802_SD_GetCardState()` with a timeout of 100,000 iterations. The non-DMA variants disable interrupts and use polling (`sd_diskio.c:160-225`).

### Card Initialization

`ADAFRUIT_802_SD_Init()` (`adafruit_802_sd.c:269-286`) performs:

1. `SD_IO_Init()` → `BSP_SPI_Init()`
2. CS high and 10 dummy bytes (80 clocks)
3. `SD_GoIdleState()`

`SD_GoIdleState()` sends CMD0, CMD8, ACMD41 and CMD58, then sets `CardType` to SDSC or SDHC (`adafruit_802_sd.c:1227-1360`). There is no card-detect pin; a missing or unresponsive card makes initialization fail. `preenfm3Init()` stores the return value in `erreurSD` and uses it for the startup error display (`preenfm3.cpp:96,121,135,148`).

### Filesystem and Storage

FatFs is used through the standard `f_open`, `f_read`, `f_write`, `f_close`, `f_opendir` and `f_readdir` APIs. The main file classes are:

- `PreenFMFileType` (`firmware/Src/filesystem/PreenFMFileType.cpp`)
- `PatchBank`, `MixerBank`, `SequenceBank`
- `ConfigurationFile`
- `UserWaveform`, `UserEnvCurve`
- `PPMImage`

The filesystem is mounted on `USERPath` after `ADAFRUIT_802_SD_Init()` succeeds. There is no hot-plug handling; SD access assumes the card was present at boot.

### Cache Coherency Note

`.ram_d2b` is MPU Region 3, configured write-back/write-allocate. SPI2 DMA reads and writes FatFS/SD buffers in that region, and there is currently no cache clean/invalidate around those transfers. This is the highest-risk cache issue in the firmware; see `doc/improvements.md`.

---

## Switches, Buttons & Encoders

### HC165 Shift Register Chain

The front-panel buttons and encoders are not read as individual GPIOs. They are scanned through a chain of 74HC165 parallel-in/serial-out shift registers.

`Encoders::getRegisterBits(encoderPush)` (`lib/Src/Encoders.cpp:92-108`):

1. Pulses LOAD low then high to latch the parallel inputs.
2. Clocks 24 bits (`encoderPush == 0`) or 32 bits (`encoderPush != 0`).
3. Reads DATA after each clock, building a 32-bit register image.
4. If a 32-bit read returns zero in the high byte, it forces those bits to `0xFF` for boards without push encoders.

Inputs are active-low: a pressed button reads as 0.

The chain contains **6 encoders** and either **12 or 18 buttons** (`lib/Inc/Encoders.h:25-27`). Encoder bit pairs and button bit positions are defined in `Encoders.cpp:25-27`.

Pin mapping is board-dependent (`lib/Src/preenfm3lib.c:61-80`):

| Board | DATA | LOAD | CLK |
|-------|------|------|-----|
| LQFP100 | PA0 | PA1 | PA2 |
| LQFP144 / LQFP176 | PF2 | PF1 | PF0 |

The `HC165_*` globals are assigned at runtime after reading the board ID from PD0–PD3. The `PE7/PE8/PE9` aliases in `firmware/Inc/main.h:64-69` are not the active mapping.

### Encoder Decoding

`encoders.checkStatus()` is called from `preenfm3Tic()` every 2 SysTick ticks, i.e. at 500 Hz (`firmware/Src/preenfm3.cpp:285-290`). It:

- Decodes the 6 quadrature encoders using a 4-bit state table (`action_[2][16]`, `Encoders.cpp:40-52`).
- Uses `encoderType` bit 0 to select the 12/24-step table and bit 1 to invert direction.
- Applies acceleration via `tickSpeed_`, capped at 12.
- Resets `lastMove_` after 60 ticks without movement.

Actions are queued in `RingBuffer<EncoderAction, 16>` and dispatched by `encoders.processActions()` from the main loop (`preenfm3.cpp:166`). Listeners receive `ENCODER_TURNED`, `ENCODER_TURNED_WHILE_BUTTON_PRESSED`, `ENCODER_BUTTON_CLICKED`, `ENCODER_TWO_BUTTON_PRESSED` and `ENCODER_LONG_BUTTON_PRESSED` events.

### Button Gestures

Button handling is in `Encoders::checkStatus()` (`Encoders.cpp:187-228`):

- **Click:** released after more than 15 scan ticks (~30 ms).
- **Long press:** held for more than 350 scan ticks (~700 ms).
- **Two-button press:** second button pressed while the first is still held.
- `buttonUsedFromSomethingElse_` prevents a button from also producing a click or long-press after it has been consumed by another gesture.

`checkStatusUpDown()` is the alternate mode used by the MIDI-controller UI; it emits `ENCODER_BUTTON_DOWN` / `ENCODER_BUTTON_UP` instead of click/long-press events.

### MIDI Controller Mode

The normal synth UI can switch to a dedicated MIDI-controller UI. The transition is triggered by the `BUTTON_PFM3_MENU` + `BUTTON_PFM3_SEQUENCER` combination (`firmware/Src/synth/SynthState.cpp:238`) and handled by `preenfm3SwitchToMidiController()` (`firmware/Src/MidiController/preenfm3MidiController.cpp:128-138`).

`midiControllerMode` values:

| Value | Meaning |
|-------|---------|
| 0 | Normal preenFM3 synth |
| 1 | MIDI controller UI |
| 2 | Transition / reinitialization |

`SysTick_Handler()` dispatches to `preenfm3Tic()` or `preenfm3MidiControllerTic()` (`firmware/Src/stm32h7xx_it.c:206-212`), and `main()` dispatches to `preenfm3Loop()` or `preenfm3MidiControllerLoop()` (`firmware/Src/main.c:184-194`). `preenfm3ExitMidiController()` reverses the transition and restores the synth listener (`preenfm3.cpp:556-567`).

`isButton1Pressed()` reads button 0 (bit 22) through the HC165 chain (`preenfm3.cpp:548-552`); the old boot-time check in `main.c:167-169` is commented out.

---

## MIDI

### DIN MIDI (USART1)

| Parameter | Value |
|-----------|-------|
| Peripheral | USART1 |
| Pins | PB6=TX, PB7=RX (`firmware/Src/stm32h7xx_hal_msp_pfm3.c:372-410`) |
| Baud rate | 31,250 |
| Format | 8 data bits, 1 stop bit, no parity, no flow control |
| FIFO | Enabled, TX/RX threshold 1/2 (`main.c:592-603`) |
| Interrupts | RXNE/RXFNE and TXE/TXNFIE enabled (`main.c:606-610`) |
| IRQ priority | 1 (`stm32h7xx_hal_msp_pfm3.c:404-406`) |

**Receive path:** `USART1_IRQHandler()` calls `HAL_UART_IRQHandler()` and then `preenfm3_USART1_IRQHandler()` (`stm32h7xx_it.c:284-293`). The custom handler reads `RDR` while RXNE is set, inserts each byte into `usartBufferIn` (a 64-byte ring buffer), and handles MIDI thru when enabled (`preenfm3.cpp:370-400`).

**Transmit path:** `MidiDecoder::sendMidiDin5Out()` enables TXEIE. The same USART1 handler drains `usartBufferOut` into `TDR` and disables TXEIE when the buffer is empty (`MidiDecoder.cpp:1238-1242`, `preenfm3.cpp:387-395`).

### USB MIDI

The USB device is a full-speed OTG FS MIDI-class device on PA11 (DM) / PA12 (DP) (`firmware/Src/usbd_conf.c:76-88`). `MX_USB_DEVICE_Init()` registers `FS_Desc_Midi`, `USBD_MIDI` and `USBD_MIDI_fops_FS` (`firmware/Src/usb_device.c:90-130`). The descriptor uses VID `0x0483`, PID `0x5740`, product string `preenfm3` (`usbd_desc_midi.c:8-14`).

**Receive path:** `dataReceived()` calls `preenfm3_usbDataReceive()` (`usbd_midi_if.c:23-36`). That function parses 4-byte USB-MIDI event packets, cable 0, strips the CIN byte and queues the MIDI bytes in the `usbMidi` ring (`preenfm3.cpp:480-535`). If MIDI thru is enabled, the same bytes are also queued to `usartBufferOut` for DIN output.

**Transmit path:** `MidiDecoder::writeMidiCCOut()` builds a 4-byte USB-MIDI packet in `usbMidiOutBuff` when `MIDICONFIG_USB == USBMIDI_IN_AND_OUT`; `sendMidiUsbOut()` transmits it on `MIDI_IN_EP` using `USBD_LL_Transmit()` (`MidiDecoder.cpp:1221-1257`).

USB MIDI modes are `Off`, `In`, and `In/Out` (`firmware/Src/hardware/Menu.h:48`, `Menu.cpp:32`).

### MIDI Parser and Routing

Both DIN and USB input are merged in `preenfm3DecodeMidiIn()` (`preenfm3.cpp:319-326`):

```cpp
while (usartBufferIn.getCount() > 0) {
    midiDecoder.newByte(usartBufferIn.remove());
}
while (usbMidi.getCount() > 0) {
    midiDecoder.newByte(usbMidi.remove());
}
```

This function is called from both SAI DMA callbacks (`preenfm3.cpp:328-349`), so MIDI decoding runs in the audio interrupt context.

`MidiDecoder::newByte()` (`MidiDecoder.cpp:76-157`):

- Handles realtime bytes (`>= 0xF8`) first: clock, start, continue, stop.
- Supports running status.
- Collects SysEx into a 32-byte buffer.
- Parses note on/off, control change, pitch bend, poly aftertouch, channel aftertouch, program change and song position.

`midiEventReceived()` (`MidiDecoder.cpp:213-350`) maps MIDI channels to timbres:

- Global channel → all timbres (except MPE instrument 1)
- Current channel → current timbre
- Per-instrument channels / omni mode
- MPE handling for instrument 1

Program changes queue a `LOAD_PRESET` async action; `processAsyncActions()` runs from `preenfm3Loop()` to load presets and NRPN data outside the audio interrupt (`MidiDecoder.cpp:1364-1382`).

MIDI clock handling is also in `newByte()`: `MIDI_CLOCK` increments the clock counter, calls `synth->midiTick()`, advances song position every 6 clocks and toggles the display indicator (`MidiDecoder.cpp:76-120`).

### MIDI Thru and Output

- DIN input can be forwarded to DIN output when `midiThru_` is enabled; the USART1 handler writes directly to `TDR` if possible, otherwise queues into `usartBufferOut`.
- USB input can be forwarded to DIN output in `preenfm3_usbDataReceive()`.
- There is no DIN-to-USB thru path in this code; USB output is generated by the MIDI decoder's control-change path.
- Buffers: `usartBufferIn` / `usartBufferOut` / `usbMidi` are 64-byte rings; `asyncActions` is 16 entries; SysEx is 32 bytes (`MidiDecoder.cpp:36-42`).

---

## Peripheral Mapping Reference

| Peripheral | Instance | Pins | Function |
|------------|----------|------|----------|
| SAI1_Block_A | SAI1 | PE2(MCLK), PE4(FS), PE5(SCK), PE6(SD) | DAC 1 (stereo) |
| SAI1_Block_B | SAI1 | PE3(SD) | DAC 1 sync |
| SAI2_Block_A | SAI2 | PD11(SD) | DAC 2 + DAC 3 |
| SPI1 | SPI1 | PA5(SCK), PA6(MISO), PA7(MOSI) | TFT display |
| SPI2 | SPI2 | PA9(SCK), PB14(MISO), PB15(MOSI), PE12(CS) | SD card (SPI mode) |
| USART1 | USART1 | PB6(TX), PB7(RX) | MIDI DIN (31250 baud) |
| USB OTG FS | USB | PA11(DM), PA12(DP) | USB MIDI |
| TIM1 CH2 | TIM1 | PE11 | TFT backlight PWM |
| TIM3 CH4 | TIM3 | PB1 | Declared but not initialized in the normal build |
| HC165 shift | GPIOA or GPIOF | LQFP100: PA0(DATA), PA1(LOAD), PA2(CLK); LQFP144/176: PF2(DATA), PF1(LOAD), PF0(CLK) | Encoder/button inputs |
| RNG | RNG | — | Hardware random number generator |
| DMA2D | DMA2D | — | 2D graphics acceleration |

---

## Memory Map

### Physical regions and linker sections

| Region | Address | Size | Linker section | Typical contents |
|--------|---------|------|----------------|------------------|
| ITCMRAM | `0x00000000` | 64 KB (linker uses 63 KB from `0x4`) | `.instruction_ram` | Code placed in ITCM |
| DTCMRAM | `0x20000000` | 128 KB | — | Stack, heap; TCM is not cached by the M7 D-cache |
| RAM_D1 | `0x24000000` | 512 KB | `.ram_d1` | TFT framebuffer/foreground/background, LFO table, some FX delay buffers |
| RAM_D2 | `0x30000000` | 128 KB | `.ram_d2` | DAC `waveform1/2/3`, PPM image, FX buffers |
| RAM_D2B | `0x30020000` | 128 KB | `.ram_d2b` | FatFS/SD/file/sequence buffers, some delay/diffuser buffers |
| RAM_D2C | `0x30040000` | 32 KB | — (unused) | No linker section and no MPU region |
| RAM_D3 | `0x38000000` | 64 KB | `.ram_d3` | Sequencer actions and step notes |
| Flash | `0x08000000` | 128 KB | — | Bootloader |
| Flash | `0x08020000` | 1920 KB | `.text`, `.rodata` | Firmware application |

The display buffers are in RAM_D1, not DTCM:

```cpp
// lib/Src/TftDisplay.cpp:33-35
RAM_D1_SECTION uint8_t  tftForeground[60000];
RAM_D1_SECTION uint16_t tftBackground[42860 * 2];
RAM_D1_SECTION uint16_t tftMemory[240 * 320];
```

The DAC waveform buffers are in RAM_D2:

```cpp
// firmware/Src/preenfm3.cpp:70-72
RAM_D2_SECTION int32_t waveform1[64 * 2];
RAM_D2_SECTION int32_t waveform2[64 * 2];
RAM_D2_SECTION int32_t waveform3[64 * 2];
```

### MPU regions

`MPU_RegionConfig()` (`firmware/Src/main.c:694-765`) configures four regions. Attribute encoding is from `HAL_MPU_ConfigRegion()`; `C=1, B=0` is write-through, `C=1, B=1` is write-back/write-allocate (see `firmware/Drivers/CMSIS/Include/mpu_armv7.h:85-86`).

| MPU region | Base | Size | TEX | C | B | S | Cache policy | Covered section / use |
|------------|------|------|-----|---|---|---|--------------|-----------------------|
| Region 1 | `0x24000000` | 512 KB | 0 | 1 | 0 | 0 | Normal, write-through | `.ram_d1` — display buffers, LFO table, FX delays |
| Region 2 | `0x30000000` | 128 KB | 0 | 1 | 0 | 0 | Normal, write-through | `.ram_d2` — DAC waveforms, PPM/FX buffers |
| Region 3 | `0x30020000` | 128 KB | 0 | 1 | 1 | 1 | Normal, write-back, write-allocate, shareable | `.ram_d2b` — FatFS/SD/file/sequence buffers |
| Region 4 | `0x38000000` | 64 KB | 0 | 1 | 1 | 1 | Normal, write-back, write-allocate, shareable | `.ram_d3` — sequencer data |

`RAM_D2C` (`0x30040000`, 32 KB), DTCM, ITCM, flash and peripherals are not covered by an explicit MPU region; they use the default memory map. The MPU is enabled with `HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT)`.

### Cache / DMA usage

The I-cache and D-cache are enabled in `firmware/Src/main.c:116-119`. The MPU is configured later, from `main.c:164`.

| Buffer(s) | Section / MPU region | Cache policy | DMA master | Notes |
|-----------|----------------------|--------------|------------|-------|
| `waveform1/2/3` | `.ram_d2` / Region 2 | Write-through | SAI1/SAI2 TX DMA | TX-only from CPU to DAC; no invalidate needed, but a barrier before DMA is still required |
| `tftMemory`, `tftForeground`, `tftBackground` | `.ram_d1` / Region 1 | Write-through | DMA2D + SPI1 TX DMA | CPU writes foreground/background, DMA2D reads them and writes `tftMemory`, then SPI1 DMA reads `tftMemory` |
| FatFS/SD/file buffers | `.ram_d2b` / Region 3 | Write-back, write-allocate | SPI2 TX/RX DMA | Needs clean before TX and invalidate after RX |
| Sequencer actions/notes | `.ram_d3` / Region 4 | Write-back, write-allocate | CPU only | No DMA coherency concern |

There is currently no application-level cache maintenance (`SCB_CleanDCache_by_Addr`, `SCB_InvalidateDCache_by_Addr`) or explicit `__DSB`/`__DMB` around these DMA transfers. See `doc/improvements.md` for the coherency problems this creates.
