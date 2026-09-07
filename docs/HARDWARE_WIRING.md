# OmniTouch AI — Hardware Wiring & Acoustic Mounting Guide

## Hardware Components (Phase 1)
* **ESP32 Development Board** (ESP32-WROOM-32 30-pin or 38-pin)
* **INMP441 Omnidirectional I2S Digital MEMS Microphone**
* **Dupont Jumper Wires** (Female-to-Female or Male-to-Female)
* **Double-sided foam tape or blue tack** (for acoustic surface coupling)

---

## Pinout & Connection Table

| INMP441 Pin | ESP32 Pin | I2S Function | Wire Recommendation | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **VDD** | **3.3V** | Power Supply | Red wire | ⚠️ **DO NOT connect to 5V/VIN** (will fry sensor) |
| **GND** | **GND** | Ground | Black wire | Common ground |
| **SD** (Serial Data) | **GPIO 32** | I2S `DATA_IN` | Blue / Yellow | Carries digital audio sample stream |
| **WS** (Word Select) | **GPIO 25** | I2S `LRCK` | Green wire | Left / Right audio frame clock |
| **SCK** (Serial Clock)| **GPIO 33** | I2S `BCLK` | White / Orange | Bit clock pulse |
| **L/R** (Channel) | **GND** | Channel Select | Tie to **GND** | Low = Left Channel, High = Right Channel |

> [!IMPORTANT]
> **L/R Pin Must Be Connected to GND!**  
> If the `L/R` pin is left floating, the microphone will intermittently toggle channels, causing silence, phase errors, or dropped audio samples.

---

## Acoustic Surface Mounting Tips

The INMP441 is a **MEMS bottom-port microphone** (there is a small acoustic hole in the center of the PCB or back of the chip).

To achieve maximum sensitivity for desk/wall taps and knocks:
1. **Direct Coupling**: Fasten the microphone PCB beneath your desk (or on the surface) using a small piece of **double-sided foam tape** or **Blu-Tack/mounting putty**.
2. **Acoustic Seal**: The foam tape helps mechanically couple the acoustic vibrations of the wood/plastic/metal surface directly into the MEMS diaphragm.
3. **Vibration Isolation for Cable**: Secure the jumper wires with a piece of tape 2–3 inches away from the sensor so pulling the wires doesn't trigger false taps.
4. **Placement**:
   - Place near the edge or under your primary working zone for local desk tapping.
   - Multiple knock patterns (Double knock, Triple knock) prevent accidental triggers from normal typing or mouse clicks.

---

## Visual Pin Layout Diagram

```
         ESP32 Dev Board                     INMP441 Module
     ┌──────────────────────┐              ┌───────────────┐
     │ 3V3 ───────────────  │─────────────▶│ VDD           │
     │ GND ───────────────  │─────────────▶│ GND           │
     │                      │      ┌──────▶│ L/R           │
     │                      │      │       │               │
     │ GPIO 32 (I2S DIN) ───│◀─────┼───────│ SD (Data Out) │
     │ GPIO 25 (I2S WS)  ───│──────┼──────▶│ WS (Word Sel) │
     │ GPIO 33 (I2S SCK) ───│──────┼──────▶│ SCK (Bit Clk) │
     │                      │      │       └───────────────┘
     │                      │      │
     │ GND ─────────────────│──────┘
     └──────────────────────┘
```

---

## Troubleshooting

1. **Serial Monitor shows only zeroes (0) or silence**:
   - Check `L/R` pin: must be firmly connected to `GND`.
   - Verify `VDD` is 3.3V, not 5V.
   - Check `SD` connected to `GPIO 32`, `WS` to `GPIO 25`, and `SCK` to `GPIO 33`.
2. **Taps are too sensitive (triggering on typing)**:
   - Increase `TAP_ENERGY_THRESHOLD` in `config.h` or via the PC Calibrator.
3. **Double taps not registering**:
   - Adjust `DOUBLE_TAP_MAX_INTERVAL_MS` (default: 400ms) to match your natural knocking speed.
