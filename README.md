# Treadmill IWT Controller (ESP32)

An ESP32-based controller for a Kettler electric treadmill that automates
Interval Walking Training (IWT). The treadmill is driven remotely over
433 MHz ASK/OOK, and a small OLED display shows live status, speed and
interval progress.

![Platform](https://img.shields.io/badge/platform-ESP32-blue)

## Features

- Fully automatic interval walking sequence: init → warm-up → 5 fast/slow
  interval pairs → cooldown
- Non-blocking RF transmission (RCSwitch) — the UI stays responsive during ramps
- Manual speed adjustment with UP/DOWN buttons; hold to auto-repeat
- Settings menu persisted in NVS (step size, base speed, phase duration,
  stop-before-start, cooldown on/off)
- SSD1306 OLED status display with progress bar; self-heals after I2C glitches
- Host-side test suite that runs the real firmware logic on a PC

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32 |
| Display | SSD1306 OLED 128×64, I2C addr `0x3C`, SDA=GPIO 8, SCL=GPIO 9 |
| RF link | 433 MHz ASK/OOK transmitter on GPIO 1 |
| Buttons | UP = GPIO 7, DOWN = GPIO 5, START = GPIO 6 (input pullup, active low) |

The treadmill must understand the 4 RF codes (`START`, `STOP`, `UP`, `DOWN`);
each UP/DOWN pulse changes speed by 0.1 km/h.

## Getting Started

### Prerequisites

- Arduino IDE or arduino-cli with ESP32 board support
- Libraries: `RCSwitch`, `Adafruit GFX`, `Adafruit SSD1306`

### Building & Flashing

1. Open `treadmill.ino` in the Arduino IDE.
2. Select your ESP32 board and port.
3. Upload.

### Running Tests (no hardware needed)

The walk-sequence logic, button handling and RF queueing can be tested on a PC:

```sh
test/run.sh
```

This compiles the actual sketch against lightweight Arduino stubs and runs
34 checks covering button timing, the full interval sequence (exact RF signal
counts), stop-mid-walk safety, the settings menu and display boot recovery
(slow SSD1306 power-up, begin() retry, late init without power cycle).

## Usage

- **Short press START** — start / stop the walking sequence
- **Short press UP/DOWN** — adjust speed by ±0.1 km/h (works any time)
- **Hold UP/DOWN** — ramp speed continuously
- **Hold START (3 s, idle)** — open settings menu; short press edits, hold saves & exits

## Documentation

Detailed design documentation, including the full walking-sequence state machine,
display layouts and pin mapping, lives in [walking-sequence.md](walking-sequence.md).

## Project Structure

```
├── treadmill.ino        # Main loop, state machine, non-blocking RF sender
├── display.cpp/.h       # OLED driver, status/menu screens, I2C recovery
├── buttons.cpp/.h       # Debounce, long-press and hold-to-repeat handling
├── settings.cpp/.h      # Settings menu + NVS persistence
├── walking-sequence.md  # Design docs (state machine, pins, displays)
└── test/                # Host-side test harness (g++, no hardware)
```

## Safety

This controls a physical treadmill. Always ensure the area around the machine
is clear before starting a sequence, use the emergency stop if available, and
verify RF behavior with the treadmill unloaded first.
