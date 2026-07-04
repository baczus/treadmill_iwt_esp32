# Projekt: Interwałowy chód na bieżni (ESP32)

Sterownik do bieżni elektrycznej (Kettler) oparty na ESP32. Komunikacja z bieżnią przez
433 MHz ASK/OOK (biblioteka RCSwitch). Wyświetlacz OLED SSD1306 128×64 (I2C, adres 0x3C, SSD1306,
piny 21/22) do prezentacji statusu, prędkości i numeru cyklu.

## Założenia

- Bieżnia rozumie 4 kody RF: `RF_START`, `RF_STOP`, `RF_UP`, `RF_DOWN`
- Każdy impuls RF zmienia prędkość o 0.1 km/h (`SIGNALS_PER_KMH = 10`)
- Przyciski: `PIN_BTN_UP` (33), `PIN_BTN_DOWN` (26), `PIN_BTN_START` (25) – wszystkie INPUT_PULLUP, active low
- Obsługa przycisków: direct `digitalRead()` z debounce 30ms (wykrywanie zbocza), bez biblioteki Button2
- Naciśnięcie START/STOP uruchamia w pełni automatyczną sekwencję marszu
- Podczas sekwencji przyciski UP/DOWN nadal działają (ręczna korekta)
- Prędkość początkowa po starcie: 1 km/h
- Rozgrzewka: do prędkości bazowej (`baseTenths`), 3 min marszu
- Interwały: `+stepSizeSignals` (szybki) / `−stepSizeSignals` (wolny), `INTERVAL_PAIRS` par po 3 min
- Wychłodzenie: gwałtowne zmniejszenie prędkości do zera (−40 sygnałów)
- Jeśli wyświetlacz nie odpowiada (brak urządzenia na 0x3C), program działa bez niego
- Wysyłka RF jest nieblokująca (`rfStart()`/`rfProcess()` w głównej pętli), wyświetlacz nie zamarza podczas TX

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
| `stepSizeSignals` | 35 (NVS) | Speed change per interval (3.5 km/h, step 0.1) |
| `baseTenths` | 40 (NVS) | Base/warmup speed (4.0 km/h) |
| `stopBeforeStart` | 1 (NVS) | Send RF_STOP before starting sequence |
| `cooldownEnabled` | 1 (NVS) | Enable rapid slowdown phase at the end |
| `phaseDurationMinutes` | 3 (NVS) | Duration per interval phase (minutes) |

## Pin mapping

| Pin | Signal | Type |
|---|---|---|
| 14 | RF transmit | output |
| 21 (SDA) | OLED I2C | |
| 22 (SCL) | OLED I2C | |
| 25 | `PIN_BTN_START` | input pullup |
| 26 | `PIN_BTN_DOWN` | input pullup |
| 33 | `PIN_BTN_UP` | input pullup |

## Display layout (128×64 OLED)

```
Row 0:  ~~~ IWT ~~~              (y=0,  size 1, centered)
Row 1:  ▓▓▓░░░░░  progress       (y=10, 5px tall, full-width)
Row 2:  <statusMsg>               (y=18, size 2, big)
Row 3:  X.X km/h                  (y=36, size 2, big)
Row 4:  <phase-dependent text>    (y=56, size 1)
```

**Row 4 content per phase:**
| Phase | Text | Progress bar |
|---|---|---|
| 1 | – | – |
| 2 | – | ✓ (20s) |
| 3 | – | ✓ (10s) |
| 4 | – | ✓ (3 min) |
| 5-6 | `Cycle: X/5` | ✓ (3 min) |
| 7 | `Cooldown...` | – |
| 8 | `Complete!` | ✓ (3s) |

Progress bar is a full-width bordered rectangle at y=10 (height=5), proportional to `elapsed / totalSec`.

## Menu display (128×64 OLED, 2 items at a time, size 2)

```
Page 0 (Step, Base):                  Page 1 (Stop, Cool):              Page 2 (Time):
 ~~ Settings ~~     (y=0,  s1)         ~~ Settings ~~     (y=0,  s1)      ~~ Settings ~~     (y=0,  s1)
 >Step 3.5          (y=12, s2)         >Stop On           (y=12, s2)      >Time 3m           (y=12, s2)
  Base 4.0          (y=30, s2)          Cool On           (y=30, s2)      (empty)            (y=30, s2)
 [OK] [Hold=Exit]   (y=50, s1)         [OK] [Hold=Exit]   (y=50, s1)      [OK] [Hold=Exit]   (y=50, s1)
```

Three pages, toggled automatically as selection moves. Items: Step (0.1 km/h), Base (0.1 km/h), Stop (On/Off), Cool (On/Off), Time (minutes).
Selected item prefixed with `>` (browse) or wrapped in `*...*` (edit).

## State machine rules

- `walkPhase == 0` = idle, `walkPhase == 1..8` = walking active
- Each phase sets `statusMsg` for the **next** phase at transition
- RF sending is non-blocking: `rfStart(queue)` schedules signals, `rfProcess()` in `loop()` sends one per call (with ~163ms interval)
- `intervalPair` starts at 0 after ramp, increments in phase 6 loop (condition `intervalPair < INTERVAL_PAIRS - 1`)
- Display shows `intervalPair + 1` for human-readable 1‑based count

## Button handling

- Direct `digitalRead()` with edge-triggered debounce (30ms)
- `ButtonIndex` enum: `BTN_UP`, `BTN_DOWN`, `BTN_START`
- `readButton(idx)` → `ButtonEvent` (NONE, SHORT_PRESS, LONG_PRESS)
- Short press on UP/DOWN: adjust `speedTenths` by ±1 and send one RF signal
- Long press (3s hold): send 10 RF signals
- `longFired[]` flag suppresses SHORT_PRESS release event after LONG_PRESS has fired
- START short-press: toggle walk sequence (start/stop)
- START long-press (idle, 5s): enter settings menu

## Settings menu

Hold `PIN_BTN_START` for 5s (only when idle, `walkActive == false`) to enter settings.

**Browse mode** (default on entry):
- UP/DOWN → select between `Step`, `Base`, `Stop`, `Cool`
- Short-press START → enter edit mode
- Long-press START (5s) → save all, exit

**Edit mode:**
- UP/DOWN → change value (auto-repeats while held)
- Short-press START → save value, back to browse
- Long-press START (5s) → save all, exit

**Settings stored in ESP32 NVS (Preferences):**
| Setting | Variable | Default | Range | Step |
|---|---|---|---|---|
| `Step` | `stepSizeSignals` | 35 (3.5 km/h) | 1–80 (0.1–8.0) | 1 (0.1 km/h) |
| `Base` | `baseTenths` | 40 (4.0 km/h) | 10–100 (1.0–10.0) | 1 (0.1 km/h) |
| `Stop` | `stopBeforeStart` | 1 (On) | 0–1 (Off/On) | toggle |
| `Cool` | `cooldownEnabled` | 1 (On) | 0–1 (Off/On) | toggle |
| `Time` | `phaseDurationMinutes` | 3 (3 min) | 1–10 (min) | 1 (min) |

## Files

| File | Purpose |
|---|---|
| `treadmill.ino` | Main loop, walking sequence state machine, non-blocking RF sender |
| `buttons.h/cpp` | `ButtonIndex` enum, `ButtonEvent` enum, `readButton()` with debounce |
| `settings.h/cpp` | Settings menu, NVS persistence, menu button handling |
| `display.h/cpp` | OLED init, main display, settings menu display |
