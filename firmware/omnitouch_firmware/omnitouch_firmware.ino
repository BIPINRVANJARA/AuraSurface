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

// Acoustic Tap Detection Thresholds (Adjustable via PC Calibrator)
float tapEnergyThreshold      = 2200.0f; // Minimum peak energy to register a knock
float tapAttackSlopeThreshold = 1200.0f; // Sharp rise required to differentiate knock from voice
uint32_t tapDebounceMs        = 75;      // Lockout time to prevent surface ring re-triggering
uint32_t multiTapWindowMs     = 380;     // Max grouping window for double / triple taps (ms)

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
float prevEnergy = 0.0f;
float lastPeakEnergy = 0.0f;
uint32_t lastTapTime = 0;
uint32_t windowStartTime = 0;
int currentTapCount = 0;
bool inTapWindow = false;
uint32_t lastTelemetryTime = 0;
uint32_t lastWifiReconnect = 0;

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

  for (size_t i = 0; i < count; i++) {
    // High-pass DC offset rejection
    dcOffset = (dcOffset * 127 + samples[i]) / 128;
    int32_t clean = samples[i] - dcOffset;
    float absVal = std::abs((float)clean);

    if (absVal > batchPeak) batchPeak = absVal;
    batchSum += absVal;
  }

  float batchAvg = batchSum / count;

  // Envelope follower
  if (batchPeak > smoothedEnergy) {
    smoothedEnergy = (smoothedEnergy * 0.3f) + (batchPeak * 0.7f);
  } else {
    smoothedEnergy = (smoothedEnergy * 0.85f) + (batchAvg * 0.15f);
  }

  float attackSlope = smoothedEnergy - prevEnergy;
  prevEnergy = smoothedEnergy;

  uint32_t now = millis();

  // Detect physical knock impulse
  if (smoothedEnergy >= tapEnergyThreshold && attackSlope >= tapAttackSlopeThreshold) {
    if ((now - lastTapTime) >= tapDebounceMs) {
      lastTapTime = now;
      lastPeakEnergy = smoothedEnergy;

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
      if (msg.indexOf("SET_THRESHOLD") >= 0) {
        int idx = msg.indexOf("\"threshold\":");
        if (idx > 0) {
          float t = msg.substring(idx + 12).toFloat();
          if (t > 100 && t < 30000) {
            tapEnergyThreshold = t;
            Serial.printf("[Config] Threshold updated to %.1f\n", t);
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
  Serial.println("[Ready] Knock or tap on surface to test.");
}

void loop() {
  wsServer.loop();

  // Multi-tap window expiration check
  uint32_t now = millis();
  if (inTapWindow && ((now - windowStartTime) >= multiTapWindowMs)) {
    inTapWindow = false;

    TapPattern pattern = TAP_NONE;
    if (currentTapCount == 1) pattern = TAP_SINGLE;
    else if (currentTapCount == 2) pattern = TAP_DOUBLE;
    else if (currentTapCount >= 3) pattern = TAP_TRIPLE;

    if (pattern != TAP_NONE) {
      TapEventMessage msg = { pattern, lastPeakEnergy };
      xQueueSend(tapEventQueue, &msg, 0);
    }
    currentTapCount = 0;
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
    char telemBuf[128];
    snprintf(telemBuf, sizeof(telemBuf),
             "{\"type\":\"TELEMETRY\",\"energy\":%.1f,\"peak\":%.1f,\"threshold\":%.1f}",
             smoothedEnergy, lastPeakEnergy, tapEnergyThreshold);
    wsServer.broadcastTXT(telemBuf);
  }

  // Auto-reconnect Wi-Fi if dropped
  if (WiFi.status() != WL_CONNECTED && (now - lastWifiReconnect > 10000)) {
    lastWifiReconnect = now;
    WiFi.reconnect();
  }
}
