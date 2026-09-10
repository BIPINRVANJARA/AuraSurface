/*
  =============================================================================
  OmniTouch AI — Unified ESP32 Firmware (Arduino IDE Compatible)
  Acoustic Surface Tap & Voice Controller using INMP441 I2S Microphone
  =============================================================================

  Hardware Pinout:
    INMP441 VDD  -> ESP32 3.3V   (DO NOT USE 5V!)
    INMP441 GND  -> ESP32 GND
    INMP441 L/R  -> ESP32 GND    (Left channel select - MUST be tied to GND)
    INMP441 SD   -> ESP32 GPIO 32 (I2S Data In)
    INMP441 WS   -> ESP32 GPIO 25 (I2S Word Select / LRCK)
    INMP441 SCK  -> ESP32 GPIO 33 (I2S Bit Clock / BCLK)
    Onboard LED  -> ESP32 GPIO 2

  Required Arduino IDE Library:
    - "WebSockets" by Markus Sattler (Install via Library Manager)
*/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <driver/i2s.h>
#include <cmath>

// =============================================================================
// User Configuration
// =============================================================================
const char* WIFI_SSID     = "vivoy200";
const char* WIFI_PASSWORD = "77777777";
const char* MDNS_HOST     = "omnitouch";          // Accessible as omnitouch.local
const uint16_t WS_PORT    = 8765;

// Pin Definitions
#define I2S_SCK_PIN   33
#define I2S_WS_PIN    25
#define I2S_SD_PIN    32
#define STATUS_LED    2

// Acoustic Tap Detection Thresholds
float knockDeltaThreshold     = 8000.0f;  // Base height above ambient structural floor
float tapAttackSlopeThreshold = 2200.0f;  // Instantaneous shockwave rise required for knock
uint32_t tapDebounceMs        = 190;      // Lockout time to prevent desk vibration ring re-triggering
uint32_t multiTapWindowMs     = 380;      // Max grouping window for double / triple taps (ms)
const int MAX_KNOCK_ZCR       = 10;       // Desk knocks on wood have <= 10 zero crossings per 128 samples
const float MIN_BAND_RATIO    = 1.35f;    // Low-pass (<350Hz) vibration MUST dominate high-pass airborne sound

// Adaptive Ambient Noise Tracking & Dynamic Scaling
float ambientNoiseFloor       = 3000.0f;  // Automatically tracks structural background noise
float baseKnockThreshold      = 8000.0f;  // Calibrated floor margin
float effectiveThreshold      = 8000.0f;  // Dynamically adjusted based on real-time room volume
bool tapArmed                 = true;     // Re-arms when quiet or after debounce window
bool soundDroppedBetweenTaps  = true;     // Rejects continuous speech/crowd from accumulating taps

// Calibration State Machine
enum SystemState {
  STATE_CALIBRATING,
  STATE_RUNNING
};

SystemState currentState          = STATE_CALIBRATING;
uint32_t calibStartTimeMs         = 0;
const uint32_t CALIB_DURATION_MS  = 3500; // 3.5 seconds room profiling
float calibEnergySum              = 0.0f;
uint32_t calibSampleCount         = 0;
float calibMaxEnergy              = 0.0f;
float calibMaxDelta               = 0.0f;
uint32_t lastLedBlinkTime         = 0;
bool ledState                     = false;

// =============================================================================
// Data Types & Globals
// =============================================================================
enum TapPattern {
  TAP_NONE = 0,
  TAP_SINGLE,
  TAP_DOUBLE,
  TAP_TRIPLE
};

const char* patternToString(TapPattern p) {
  switch (p) {
    case TAP_SINGLE: return "SINGLE_TAP";
    case TAP_DOUBLE: return "DOUBLE_TAP";
    case TAP_TRIPLE: return "TRIPLE_TAP";
    default: return "NONE";
  }
}

struct TapEventMessage {
  TapPattern pattern;
  float energy;
};

QueueHandle_t tapEventQueue = nullptr;
WebSocketsServer wsServer(WS_PORT);

// Tap Detector State
int32_t dcOffset = 0;
float smoothedEnergy = 0.0f;
float smoothedLpEnergy = 0.0f;
float prevLpEnergy = 0.0f;
float lastPeakEnergy = 0.0f;
uint32_t lastTapTime = 0;
uint32_t windowStartTime = 0;
int currentTapCount = 0;
bool inTapWindow = false;
uint32_t lastTelemetryTime = 0;
uint32_t lastWifiReconnect = 0;

// Dual-Band Filter & Spectral Analysis States
int32_t prevCleanSample = 0;
float lpFilter1 = 0.0f;
float lpFilter2 = 0.0f;
int lastBatchZcr = 0;
float lastBandRatio = 1.0f;
float lastLpEnergy = 0.0f;
float lastHpEnergy = 0.0f;

// =============================================================================
// I2S Driver Setup
// =============================================================================
bool setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  if (i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL) != ESP_OK) {
    Serial.println("[I2S] Driver install failed!");
    return false;
  }
  if (i2s_set_pin(I2S_NUM_0, &pin_config) != ESP_OK) {
    Serial.println("[I2S] Pin configuration failed!");
    return false;
  }
  Serial.println("[I2S] INMP441 successfully initialized at 16kHz.");
  return true;
}

// =============================================================================
// Signal Processing & Tap Recognition
// =============================================================================
void processAudioSamples(const int16_t* samples, size_t count) {
  if (!samples || count == 0) return;

  float batchPeak = 0.0f;
  float batchSum = 0.0f;
  float lpBatchPeak = 0.0f;
  float lpBatchSum = 0.0f;
  float hpBatchSum = 0.0f;
  int zcr = 0;

  for (size_t i = 0; i < count; i++) {
    // High-pass DC offset rejection
    dcOffset = (dcOffset * 127 + samples[i]) / 128;
    int32_t clean = samples[i] - dcOffset;
    float absClean = std::abs((float)clean);

    if (absClean > batchPeak) batchPeak = absClean;
    batchSum += absClean;

    // Zero-crossing rate: voice and music oscillate rapidly (ZCR > 15-60)
    // Mechanical desk knocks are low-frequency thuds (ZCR <= 10)
    if ((clean >= 0 && prevCleanSample < 0) || (clean < 0 && prevCleanSample >= 0)) {
      zcr++;
    }
    prevCleanSample = clean;

    // 2-pole IIR Low-Pass filter (< 350 Hz) for extracting solid desk mechanical vibrations
    // At 16 kHz: alpha = 0.13
    lpFilter1 += 0.13f * ((float)clean - lpFilter1);
    lpFilter2 += 0.13f * (lpFilter1 - lpFilter2);
    float lpVal = lpFilter2;
    float hpVal = (float)clean - lpVal;

    float absLp = std::abs(lpVal);
    float absHp = std::abs(hpVal);

    if (absLp > lpBatchPeak) lpBatchPeak = absLp;
    lpBatchSum += absLp;
    hpBatchSum += absHp;
  }

  lastBatchZcr = zcr;
  lastLpEnergy = lpBatchSum / count;
  lastHpEnergy = hpBatchSum / count;
  lastBandRatio = (hpBatchSum > 10.0f) ? (lpBatchSum / hpBatchSum) : 3.0f;
  float batchAvg = batchSum / count;

  // Envelope follower for low-frequency structural vibration
  if (lpBatchPeak > smoothedLpEnergy) {
    smoothedLpEnergy = (smoothedLpEnergy * 0.25f) + (lpBatchPeak * 0.75f);
  } else {
    smoothedLpEnergy = (smoothedLpEnergy * 0.85f) + (lastLpEnergy * 0.15f);
  }

  // Broadband envelope for telemetry
  if (batchPeak > smoothedEnergy) {
    smoothedEnergy = (smoothedEnergy * 0.25f) + (batchPeak * 0.75f);
  } else {
    smoothedEnergy = (smoothedEnergy * 0.85f) + (batchAvg * 0.15f);
  }

  // Crest factor of structural vibration
  float crestFactor = lastLpEnergy > 10.0f ? (lpBatchPeak / lastLpEnergy) : 1.0f;

  // ---------------------------------------------------------------------------
  // STATE 1: CALIBRATING (Profile structural ambient floor & surges)
  // ---------------------------------------------------------------------------
  if (currentState == STATE_CALIBRATING) {
    calibEnergySum += smoothedLpEnergy;
    calibSampleCount++;
    if (smoothedLpEnergy > calibMaxEnergy) calibMaxEnergy = smoothedLpEnergy;

    if (smoothedLpEnergy < ambientNoiseFloor) {
      ambientNoiseFloor = (ambientNoiseFloor * 0.90f) + (smoothedLpEnergy * 0.10f);
    } else {
      ambientNoiseFloor = (ambientNoiseFloor * 0.96f) + (smoothedLpEnergy * 0.04f);
    }

    float delta = smoothedLpEnergy - ambientNoiseFloor;
    if (delta > calibMaxDelta) calibMaxDelta = delta;

    prevLpEnergy = smoothedLpEnergy;
    return; // Block tap events while calibrating
  }

  // ---------------------------------------------------------------------------
  // STATE 2: RUNNING (Dynamic Sensitivity & Airborne Sound Immunity)
  // ---------------------------------------------------------------------------
  effectiveThreshold = std::max(baseKnockThreshold, ambientNoiseFloor * 1.25f);
  knockDeltaThreshold = effectiveThreshold;

  // Track room ambient structural noise floor when not in tap impulse
  if (tapArmed) {
    if (smoothedLpEnergy < ambientNoiseFloor) {
      ambientNoiseFloor = (ambientNoiseFloor * 0.90f) + (smoothedLpEnergy * 0.10f);
    } else {
      ambientNoiseFloor = (ambientNoiseFloor * 0.980f) + (smoothedLpEnergy * 0.020f);
    }
  }

  float attackSlope = smoothedLpEnergy - prevLpEnergy;
  prevLpEnergy = smoothedLpEnergy;

  // Mechanical impulse height above baseline
  float knockDelta = smoothedLpEnergy - ambientNoiseFloor;
  if (knockDelta < 0.0f) knockDelta = 0.0f;

  uint32_t now = millis();

  // Track if sound decayed back towards quiet baseline
  if (knockDelta < (effectiveThreshold * 0.40f)) {
    soundDroppedBetweenTaps = true;
    tapArmed = true;
  } else if ((now - lastTapTime) >= tapDebounceMs) {
    tapArmed = true;
  }

  // Detect genuine physical desk knock:
  // 1. Must spike above structural baseline (knockDelta >= effectiveThreshold)
  // 2. Must have steep shockwave attack (attackSlope >= tapAttackSlopeThreshold)
  // 3. Must have impulsive crest factor (crestFactor >= 1.8f)
  // 4. Must be LOW FREQUENCY: ZCR <= 10 (Voice & Music have ZCR 20-60+)
  // 5. Low-pass structural energy must dominate airborne high-pass: lastBandRatio >= 1.35f
  bool isMechanicalImpulse = (knockDelta >= effectiveThreshold) &&
                             (attackSlope >= tapAttackSlopeThreshold) &&
                             (crestFactor >= 1.8f) &&
                             (zcr <= MAX_KNOCK_ZCR) &&
                             (lastBandRatio >= MIN_BAND_RATIO);

  if (tapArmed && isMechanicalImpulse) {
    if ((now - lastTapTime) >= tapDebounceMs) {
      if (inTapWindow && !soundDroppedBetweenTaps) {
        return; // Continuous sound rejection
      }

      lastTapTime = now;
      tapArmed = false;
      soundDroppedBetweenTaps = false;
      lastPeakEnergy = smoothedLpEnergy;

      if (!inTapWindow) {
        inTapWindow = true;
        windowStartTime = now;
        currentTapCount = 1;
      } else {
        currentTapCount++;
      }
    }
  }
}

// Dedicated FreeRTOS Core 0 Task for Audio DSP
void audioTask(void* param) {
  const size_t CHUNK = 128;
  int32_t rawBuf[CHUNK];
  int16_t pcmBuf[CHUNK];

  while (true) {
    size_t bytesRead = 0;
    esp_err_t err = i2s_read(I2S_NUM_0, (void*)rawBuf, CHUNK * sizeof(int32_t), &bytesRead, portMAX_DELAY);
    if (err == ESP_OK && bytesRead > 0) {
      size_t samples = bytesRead / sizeof(int32_t);
      for (size_t i = 0; i < samples; i++) {
        int32_t val = rawBuf[i] >> 14;
        if (val > 32767) val = 32767;
        if (val < -32768) val = -32768;
        pcmBuf[i] = (int16_t)val;
      }
      processAudioSamples(pcmBuf, samples);
    }
    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// =============================================================================
// Calibration Controller
// =============================================================================
void startCalibration() {
  currentState = STATE_CALIBRATING;
  calibStartTimeMs = millis();
  calibEnergySum = 0.0f;
  calibSampleCount = 0;
  calibMaxEnergy = 0.0f;
  calibMaxDelta = 0.0f;
  inTapWindow = false;
  currentTapCount = 0;
  Serial.println("\n[Calibration] Profiling background acoustic noise (3.5s)...");
  wsServer.broadcastTXT("{\"type\":\"CALIBRATION_START\",\"duration\":3500}");
}

// =============================================================================
// WebSocket Event Handler
// =============================================================================
void onWsEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      IPAddress ip = wsServer.remoteIP(num);
      Serial.printf("[WebSocket] Client [%u] connected from %s\n", num, ip.toString().c_str());
      wsServer.sendTXT(num, "{\"type\":\"INFO\",\"device\":\"OmniTouch_ESP32\",\"version\":\"1.0\"}");
      break;
    }
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Client [%u] disconnected\n", num);
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload).substring(0, length);
      if (msg.indexOf("CALIBRATE") >= 0) {
        startCalibration();
      } else if (msg.indexOf("SET_THRESHOLD") >= 0) {
        int idx = msg.indexOf("\"threshold\":");
        if (idx > 0) {
          float t = msg.substring(idx + 12).toFloat();
          if (t > 100 && t < 40000) {
            baseKnockThreshold = t;
            knockDeltaThreshold = t;
            effectiveThreshold = t;
            Serial.printf("[Config] Knock Threshold manually updated to %.1f\n", t);
          }
        }
      }
      break;
    }
    default:
      break;
  }
}

void pulseLed() {
  digitalWrite(STATUS_LED, HIGH);
  delay(35);
  digitalWrite(STATUS_LED, LOW);
}

// =============================================================================
// Setup & Main Loop
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- OmniTouch AI ESP32 Controller ---");

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  tapEventQueue = xQueueCreate(10, sizeof(TapEventMessage));

  if (!setupI2S()) {
    while (1) { delay(1000); }
  }

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 8000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin(MDNS_HOST)) {
      Serial.printf("[mDNS] Responder: http://%s.local\n", MDNS_HOST);
      MDNS.addService("omnitouch", "tcp", WS_PORT);
    }
  } else {
    Serial.println("[WiFi] Not connected yet. Running offline tap detection.");
  }

  // Start WebSocket Server
  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  // Launch Core 0 Audio Task
  xTaskCreatePinnedToCore(audioTask, "audioTask", 4096, NULL, 2, NULL, 0);

  pulseLed();
  Serial.println("[Ready] Starting initial auto-calibration...");
  startCalibration();
}

void loop() {
  wsServer.loop();

  uint32_t now = millis();

  // Calibration completion check & fast status LED blinking
  if (currentState == STATE_CALIBRATING) {
    if (now - lastLedBlinkTime >= 90) {
      lastLedBlinkTime = now;
      ledState = !ledState;
      digitalWrite(STATUS_LED, ledState ? HIGH : LOW);
    }

    if ((now - calibStartTimeMs) >= CALIB_DURATION_MS) {
      currentState = STATE_RUNNING;
      float avgFloor = calibSampleCount > 0 ? (calibEnergySum / calibSampleCount) : ambientNoiseFloor;
      ambientNoiseFloor = avgFloor;

      // Base knock threshold: comfortable margin above the highest room surge observed
      baseKnockThreshold = std::max(6000.0f, (calibMaxDelta * 1.30f) + 2500.0f);
      effectiveThreshold = std::max(baseKnockThreshold, ambientNoiseFloor * 1.25f);
      knockDeltaThreshold = effectiveThreshold;

      // Double confirmation flash
      digitalWrite(STATUS_LED, HIGH); delay(100); digitalWrite(STATUS_LED, LOW); delay(60);
      digitalWrite(STATUS_LED, HIGH); delay(100); digitalWrite(STATUS_LED, LOW);

      Serial.println("\n======================================================");
      Serial.println("  [OmniTouch Calibration Complete] System Online!     ");
      Serial.printf("  * Measured Ambient Floor: %.1f\n", ambientNoiseFloor);
      Serial.printf("  * Max Background Spike:   %.1f\n", calibMaxDelta);
      Serial.printf("  * Calibrated Threshold:   %.1f\n", effectiveThreshold);
      Serial.println("======================================================\n");

      char calibMsg[180];
      snprintf(calibMsg, sizeof(calibMsg),
               "{\"type\":\"CALIBRATION_COMPLETE\",\"floor\":%.1f,\"maxSpike\":%.1f,\"threshold\":%.1f}",
               ambientNoiseFloor, calibMaxDelta, effectiveThreshold);
      Serial.println(calibMsg);
      wsServer.broadcastTXT(calibMsg);
    }
  }

  // Read commands sent over USB Serial (e.g. from PC Agent or Calibrator)
  if (Serial.available()) {
    String s = Serial.readStringUntil('\n');
    s.trim();
    if (s.indexOf("CALIBRATE") >= 0) {
      startCalibration();
    } else if (s.indexOf("SET_THRESHOLD") >= 0) {
      int idx = s.indexOf("\"threshold\":");
      if (idx > 0) {
        float t = s.substring(idx + 12).toFloat();
        if (t > 100 && t < 40000) {
          baseKnockThreshold = t;
          knockDeltaThreshold = t;
          effectiveThreshold = t;
          Serial.printf("[Config] Knock Threshold manually updated via Serial to %.1f\n", t);
        }
      }
    }
  }

  // Multi-tap window expiration check: emit when user finishes tapping or reaches triple tap
  if (inTapWindow) {
    if (currentTapCount >= 3 || ((now - lastTapTime) >= multiTapWindowMs)) {
      TapPattern pattern = TAP_NONE;
      if (currentTapCount == 1) pattern = TAP_SINGLE;
      else if (currentTapCount == 2) pattern = TAP_DOUBLE;
      else if (currentTapCount >= 3) pattern = TAP_TRIPLE;

      inTapWindow = false;
      currentTapCount = 0;

      if (pattern != TAP_NONE) {
        TapEventMessage msg = { pattern, lastPeakEnergy };
        xQueueSend(tapEventQueue, &msg, 0);
      }
    }
  }

  // Process and broadcast detected tap events
  TapEventMessage evt;
  if (xQueueReceive(tapEventQueue, &evt, 0) == pdTRUE) {
    const char* patStr = patternToString(evt.pattern);
    Serial.printf("\n>>> [TAP DETECTED] %s (Energy: %.1f) <<<\n", patStr, evt.energy);
    pulseLed();

    char jsonBuf[128];
    snprintf(jsonBuf, sizeof(jsonBuf),
             "{\"type\":\"EVENT\",\"source\":\"SURFACE\",\"pattern\":\"%s\",\"energy\":%.1f}",
             patStr, evt.energy);
    Serial.println(jsonBuf);
    wsServer.broadcastTXT(jsonBuf);
  }

  // Telemetry stream (every 50ms) for real-time visual calibration
  if (now - lastTelemetryTime >= 50) {
    lastTelemetryTime = now;
    float delta = smoothedLpEnergy - ambientNoiseFloor;
    char telemBuf[200];
    snprintf(telemBuf, sizeof(telemBuf),
             "{\"type\":\"TELEMETRY\",\"state\":\"%s\",\"energy\":%.1f,\"lp\":%.1f,\"zcr\":%d,\"ratio\":%.2f,\"floor\":%.1f,\"delta\":%.1f,\"threshold\":%.1f}",
             currentState == STATE_CALIBRATING ? "CALIBRATING" : "RUNNING",
             smoothedEnergy, smoothedLpEnergy, lastBatchZcr, lastBandRatio, ambientNoiseFloor, delta, effectiveThreshold);
    wsServer.broadcastTXT(telemBuf);
  }

  // Auto-reconnect Wi-Fi if dropped
  if (WiFi.status() != WL_CONNECTED && (now - lastWifiReconnect > 10000)) {
    lastWifiReconnect = now;
    WiFi.reconnect();
  }
}
