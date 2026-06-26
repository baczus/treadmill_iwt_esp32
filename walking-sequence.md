# Projekt: Interwałowy chód na bieżni (ESP32)

Sterownik do bieżni elektrycznej (Kettler) oparty na ESP32. Komunikacja z bieżnią przez
433 MHz ASK/OOK (biblioteka RCSwitch). Wyświetlacz OLED SSD1306 128×32 (I2C, adres 0x3C,
piny 21/22) do prezentacji statusu, prędkości i numeru cyklu.

## Założenia

- Bieżnia rozumie 4 kody RF: `RF_START`, `RF_STOP`, `RF_UP`, `RF_DOWN`
- Każdy impuls RF zmienia prędkość o 0.1 km/h (`SIGNALS_PER_KMH = 10`)
- Przyciski: `PIN_BTN_UP` (33), `PIN_BTN_DOWN` (26), `PIN_BTN_START` (25) – wszystkie INPUT_PULLUP
- Naciśnięcie START/STOP uruchamia w pełni automatyczną sekwencję marszu
- Podczas sekwencji przyciski UP/DOWN nadal działają (ręczna korekta)
- Prędkość początkowa po starcie: 1 km/h
- Rozgrzewka: do prędkości bazowej (`baseTenths`), 3 min marszu
- Interwały: `+stepSizeSignals` (szybki) / `−stepSizeSignals` (wolny), `INTERVAL_PAIRS` par po 3 min
- Wychłodzenie: gwałtowne zmniejszenie prędkości do zera (−40 sygnałów)
- Jeśli wyświetlacz nie odpowiada (brak urządzenia na 0x3C), program działa bez niego
- Wysyłka RF blokuje pętlę główną – wyświetlacz zamarza podczas TX

# Walking Sequence – State Machine (8 phases)

| Phase | Action | Duration | Speed (km/h) | statusMsg (row 1) | Row 3 |
|---|---|---|---|---|---|
| 1 – STOP | send `RF_STOP` | instant | 0 | `Init` | — |
| 2 – START | wait 20s, send `RF_START` | 20s | 1.0 | `Starting` | — |
| 3 – RAMP | wait 10s, ramp 1→base | 10s + TX | **base** | →`Warm-up` | — |
| 4 – WARMUP | wait 3 min | 3 min | **base** | `Warm-up` | — |
| 4→5 | `+stepSizeSignals` | TX | **base+step** | →`Fast` | — |
| 5 – FAST | wait 3 min | 3 min | **base+step** | `Fast` | `Cycle: X/5` |
| 5→6 | `−stepSizeSignals` | TX | **base** | →`Slow` | — |
| 6 – SLOW | wait 3 min | 3 min | **base** | `Slow` | `Cycle: X/5` |
| 6→5 *(×4)* | `+stepSizeSignals` | TX | **base+step** | →`Fast` | cycle 2/5…5/5 |
| 6→7 (exit) | — | — | — | →`Cooling` | — |
| 7 – COOLDOWN | rapid slowdown (−40 sig, if enabled) | ~8s TX | →0 | `Cooling` | `Cooldown...` |
| 8 – COMPLETE | wait 3s, reset | 3s | 0 | →`Ready` | `Complete!` |

## Key constants & variables

| Name | Default | Purpose |
|---|---|---|
| `SIGNALS_PER_KMH` | 10 | RF signals per 1 km/h change |
| `START_SPEED_TENTHS` | 1 | Speed after START (km/h) |
| `INTERVAL_PAIRS` | 5 | Fast+Slow interval pairs |
| `PHASE_DURATION_MS` | 180000 | Duration per interval phase (ms) |
| `stepSizeSignals` | 35 (NVS) | Speed change per interval (3.5 km/h) |
| `baseTenths` | 40 (NVS) | Base/warmup speed (4.0 km/h) |
| `stopBeforeStart` | 1 (NVS) | Send RF_STOP before starting sequence |
| `cooldownEnabled` | 1 (NVS) | Enable rapid slowdown phase at the end |

## Pin mapping

| Pin | Signal | Type |
|---|---|---|
| 14 | RF transmit | output |
| 21 (SDA) | OLED I2C | |
| 22 (SCL) | OLED I2C | |
| 25 | `PIN_BTN_START` | input pullup |
| 26 | `PIN_BTN_DOWN` | input pullup |
| 33 | `PIN_BTN_UP` | input pullup |

## Display layout (128×32 OLED, 4 rows)

```
Row 0:  ~~~ IWT ~~~              (centered x=31)
Row 1:  <statusMsg>               (x=0, len ≤ 21 chars)
Row 2:  Speed: X.X km/h           (x=0)
Row 3:  <phase-dependent text>    (x=0)
        ▓▓░░░░░░░░  progress bar  (y=29, 3px tall)
```

**Row 3 content per phase:**
| Phase | Text | Progress bar |
|---|---|---|
| 1 | – | – |
| 2 | – | ✓ (20s) |
| 3 | – | ✓ (10s) |
| 4 | – | ✓ (3 min) |
| 5-6 | `Cycle: X/5` | ✓ (3 min) |
| 7 | `Cooldown...` | – |
| 8 | `Complete!` | ✓ (3s) |

Progress bar is a filled rectangle at y=29 (height=3), proportional to `elapsed / totalSec`.

## State machine rules

- `walkPhase == 0` = idle, `walkPhase == 1..8` = walking active
- Each phase sets `statusMsg` for the **next** phase at transition
- `sendRfCode()` blocks the loop during TX
- `intervalPair` starts at 0 after ramp, increments in phase 6 loop (condition `intervalPair < INTERVAL_PAIRS - 1`)
- Display shows `intervalPair + 1` for human-readable 1‑based count

## Settings menu

Hold `PIN_BTN_START` for 5s (only when idle, `walkActive == false`) to enter settings.

**Browse mode** (default on entry):
- UP/DOWN → select between `Step` and `Base`
- Short-press START → enter edit mode
- Long-press START (5s) → save all, exit

**Edit mode:**
- UP/DOWN → change value (auto-repeats while held)
- Short-press START → save value, back to browse
- Long-press START (5s) → save all, exit

**Settings stored in ESP32 NVS (Preferences):**
| Setting | Variable | Default | Range | Step |
|---|---|---|---|---|
| `Step` | `stepSizeSignals` | 35 (3.5 km/h) | 5–80 (0.5–8.0) | 5 (0.5 km/h) |
| `Base` | `baseTenths` | 40 (4.0 km/h) | 10–100 (1.0–10.0) | 1 (0.1 km/h) |
| `Stop` | `stopBeforeStart` | 1 (On) | 0–1 (Off/On) | toggle |
| `Cool` | `cooldownEnabled` | 1 (On) | 0–1 (Off/On) | toggle |

## Files

| File | Purpose |
|---|---|
| `treadmill.ino` | Main loop, walking sequence state machine |
| `buttons.h/cpp` | Button struct, readButton(), long-press detection |
| `settings.h/cpp` | Settings menu, NVS persistence, menu button handling |
| `display.h/cpp` | OLED init, main display, menu display |
