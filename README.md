# 🖐️ OmniTouch AI — Haptic Surface & Voice Controller

Transform ordinary desks and surfaces into programmable human-computer interfaces using acoustic vibration sensing, voice commands, and zero-latency Windows PC automation.

---

## ⚡ Quick Hardware Setup (ESP32 + INMP441)

Connect the **INMP441 I2S microphone** to your **ESP32**:

| INMP441 Pin | ESP32 Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **VDD** | **3.3V** | Power | ⚠️ **Do NOT connect to 5V!** |
| **GND** | **GND** | Ground | Common Ground |
| **L/R** | **GND** | Channel Select | **Must be tied to GND** (Left channel) |
| **SD** | **GPIO 32** | I2S Serial Data | Digital audio stream |
| **WS** | **GPIO 25** | I2S Word Select | Frame clock |
| **SCK** | **GPIO 33** | I2S Bit Clock | Clock pulse |

### 🪵 Desk Mounting Tip
Tape the microphone beneath your desk surface using double-sided foam tape or mounting putty. Direct mechanical contact allows the acoustic sensor to pick up surface vibrations and knocks while filtering out background airborne noise.

---

## 🚀 Flashing the Firmware

### Option A: Arduino IDE (Easiest)
1. Open Arduino IDE.
2. In **Tools > Manage Libraries...**, install:
   - **WebSockets** by *Markus Sattler*
3. Open `firmware/omnitouch_firmware/omnitouch_firmware.ino`.
4. In lines 26–27, set your Wi-Fi details:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
5. Select Board: **ESP32 Dev Module** and Port, then click **Upload**.

### Option B: PlatformIO (VS Code)
1. Open the `firmware/` folder in VS Code with PlatformIO extension installed.
2. Edit Wi-Fi credentials in `firmware/include/config.h`.
3. Click PlatformIO **Build** and **Upload**.

---

## 💻 Running the Windows PC Agent

1. Open a terminal in `pc_agent/`:
   ```bash
   cd pc_agent
   pip install -r requirements.txt
   ```
2. Launch the agent:
   ```bash
   python src/agent.py
   ```
   *Or double-click `start_agent.bat`.*

### Default Haptic Taps & Gestures:
| Action | Gesture / Knock Pattern |
| :--- | :--- |
| **Play / Pause Media** | Single Tap on desk |
| **Mute / Unmute Microphone** | Double Knock on desk |
| **Snipping Tool (Screenshot)** | Triple Knock on desk |

### Spoken Voice Commands:
- *"mute"* / *"mute microphone"* ➔ Toggles system mic mute
- *"play"* / *"pause"* ➔ Media play/pause
- *"next song"* ➔ Next track
- *"previous song"* ➔ Previous track
- *"volume up"* / *"volume down"* ➔ Master volume adjustments
- *"screenshot"* ➔ Snipping tool

---

## 🎯 Real-Time Tap Calibrator

Every desk has different thickness and acoustic resonance. Use the live calibrator to see your desk's vibrations and dial in the sensitivity:

```bash
cd pc_agent
python src/calibrator.py ws://omnitouch.local:8765
```
*Or double click `start_calibrator.bat`.*

A live visual meter will show current vibration level vs. the trigger threshold:
```text
Level: [████████████|░░░░░░░░░░░░░░░░░░░░░░]  Energy: 2450 | Peak: 3100 | Thresh: 2200
>>> [EVENT DETECTED] DOUBLE_TAP | Peak: 3450 <<<
```

---

## 🧪 Testing Without Hardware (Simulator Mode)

You can test the entire PC agent and all Windows controls right now without plugging in the ESP32:

1. In one terminal, start the virtual ESP32 simulator:
   ```bash
   python simulator/mock_esp32_server.py
   ```
2. In a second terminal, start the PC agent:
   ```bash
   python pc_agent/src/agent.py
   ```
3. In the simulator terminal, press:
   - `1` ➔ Simulates **Single Tap** (Media Play/Pause)
   - `2` ➔ Simulates **Double Tap** (Microphone Mute toggle)
   - `3` ➔ Simulates **Triple Tap** (Screenshot tool)
