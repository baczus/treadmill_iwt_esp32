# Projekt: Interwałowy chód na bieżni (ESP32)

Sterownik do bieżni elektrycznej (Kettler) oparty na ESP32. Komunikacja z bieżnią przez
433 MHz ASK/OOK (biblioteka RCSwitch). Wyświetlacz OLED SSD1306 128×32 (I2C, adres 0x3C,
piny 21/22) do prezentacji statusu, prędkości i numeru cyklu.

## Założenia

- Bieżnia rozumie 4 kody RF: START, STOP, SPEED_UP, SPEED_DOWN
- Każdy impuls RF zmienia prędkość o 0.1 km/h (stała `SIGNALS_PER_KMH = 10` na 1 km/h)
- Przyciski: SPEED_UP (pin 33), SPEED_DOWN (pin 26), START/STOP (pin 25) – wszystkie z pull-up
- Naciśnięcie START/STOP uruchamia w pełni automatyczną sekwencję marszu
- Podczas sekwencji przyciski SPEED_UP/DOWN nadal działają (ręczna korekta)
- Prędkość początkowa po starcie: 1 km/h
- Rozgrzewka: +3 km/h → 4 km/h, 3 min marszu
- Interwały: +3 km/h (szybki) / −3 km/h (wolny), 5 par po 3 min każda
- Wychłodzenie: gwałtowne zmniejszenie prędkości do zera (−40 sygnałów)
- Jeśli wyświetlacz nie odpowiada (brak urządzenia na 0x3C), program działa bez niego
- Wysyłka RF blokuje pętlę główną na ~6s (30 sygnałów × 200ms) – wyświetlacz zamarza

# Walking Sequence – State Machine (8 phases)

| Phase | Action | Duration | Speed (km/h) | statusText (row 1) | Row 3 |
|---|---|---|---|---|---|
| 1 – STOP | send STOP signal | instant | 0 | `Starting` | — |
| 2 – START | wait 20s, send START | 20s | 1.0 | `Starting` | — |
| 3 – RAMP | wait 10s, +3 km/h (1→4) | 10s + ~6s TX | **4.0** | →`Warm-up` (po +3) | — |
| 4 – WARMUP | wait 3 min at 4.0 | 3 min | **4.0** | `Warm-up` | — |
| 4→5 | +3 km/h (4→7) | ~6s TX | **7.0** | →`Fast` | — |
| 5 – FAST | wait 3 min at 7.0 | 3 min | **7.0** | `Fast` | `Cycle: 1/5` |
| 5→6 | −3 km/h (7→4) | ~6s TX | **4.0** | →`Slow` | — |
| 6 – SLOW | wait 3 min at 4.0 | 3 min | **4.0** | `Slow` | `Cycle: 1/5` |
| 6→5 *(×4)* | +3 km/h (4→7) | ~6s TX | **7.0** | →`Fast` | cykl 2/5…5/5 |
| 6→7 (exit) | — | — | — | →`Cooling` | — |
| 7 – COOLDOWN | rapid slowdown (−40 sig) | ~8s TX | →0 | `Cooling` | `Cooldown...` |
| 8 – COMPLETE | wait 3s, reset | 3s | 0 | `Cooling`→`Ready` | `Complete!` |

## Key variables

| Name | Value | Purpose |
|---|---|---|
| `SIGNALS_PER_KMH` | 10 | RF signals per 1 km/h change |
| `MIN_TREADMILL_SPEED` | 1 | Min speed after START (km/h) |
| `WARMUP_SPEED` | 3 | km/h added during ramp (1→4) |
| `SPEED_STEP` | 3 | km/h change per interval |
| `CYCLE_COUNT` | 5 | Fast+Slow interval pairs |
| `INTERVAL_3MIN` | 180000 | Wait duration per phase (ms) |

## Pin mapping

| Pin | Signal | Type |
|---|---|---|
| 14 | RF transmit | output |
| 21 (SDA) | OLED I2C | |
| 22 (SCL) | OLED I2C | |
| 25 | START/STOP button | input pullup |
| 26 | SPEED DOWN button | input pullup |
| 33 | SPEED UP button | input pullup |

## Display layout (128×32 OLED, 4 rows)

```
Row 0:  ~~~ IWT ~~~              (centered x=31)
Row 1:  <statusText>              (x=0, len ≤ 21 chars)
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

Progress bar is a filled rectangle at the bottom of the display (y=29, height=3),
proportional to `elapsed / totalSec`.

## State machine rules

- `seqPhase=0` = idle, `seqPhase=1..8` = walking active
- Each phase sets statusText for the **next** phase at transition
- SendCode blocks the loop (~6s per 30 signals @ 200ms interval) – display freezes during TX
- `seqCycle` starts at 0 after ramp, increments in phase 6 loop (condition `seqCycle < CYCLE_COUNT-1`)
- Display shows `seqCycle+1` for human-readable 1‑based count
