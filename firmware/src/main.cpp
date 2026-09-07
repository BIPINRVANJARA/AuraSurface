#include <Arduino.h>
#include "config.h"
#include "i2s_sampler.h"
#include "acoustic_tap_detector.h"
#include "network_manager.h"
#include "ws_server.h"

// Global component instances
I2SSampler sampler;
AcousticTapDetector tapDetector;
NetworkManager networkMgr;
WSServer wsServer(WEBSOCKET_PORT);

// Queue for passing detected tap events from Audio task to Network task
struct TapEventMessage {
    TapPattern pattern;
    float energy;
};
QueueHandle_t tapEventQueue = nullptr;

// Audio processing task running on Core 0
void audioTask(void* parameter) {
    const size_t BATCH_SIZE = 128;
    int16_t sampleBuffer[BATCH_SIZE];

    Serial.println("[Core 0] Audio Acquisition & DSP Task running.");

    while (true) {
        // Read samples from INMP441 via I2S
        size_t samplesRead = sampler.read(sampleBuffer, BATCH_SIZE);
        if (samplesRead > 0) {
            tapDetector.processSamples(sampleBuffer, samplesRead);
        }

        // Yield briefly to keep watchdog happy
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
}

// Visual pulse on onboard LED
void pulseLED() {
    digitalWrite(STATUS_LED_PIN, HIGH);
    delay(40);
    digitalWrite(STATUS_LED_PIN, LOW);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("==================================================");
    Serial.println("         OmniTouch AI — ESP32 Firmware v1.0       ");
    Serial.println("   Acoustic Surface Tap & Voice Controller        ");
    Serial.println("==================================================");

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    // Create FreeRTOS queue for tap events
    tapEventQueue = xQueueCreate(10, sizeof(TapEventMessage));

    // Configure tap detector callbacks
    tapDetector.setThresholds(TAP_ENERGY_THRESHOLD, TAP_ATTACK_SLOPE_THRESHOLD, TAP_DEBOUNCE_MS, MULTI_TAP_WINDOW_MS);

    tapDetector.setCallbacks(
        [](TapPattern pattern, float peakEnergy) {
            TapEventMessage msg = { pattern, peakEnergy };
            if (tapEventQueue != nullptr) {
                xQueueSend(tapEventQueue, &msg, 0);
            }
        },
        [](float liveEnergy, float peakEnergy) {
            // Live telemetry for calibration tool
            wsServer.broadcastTelemetry(liveEnergy, peakEnergy, tapDetector.getEnergyThreshold());
        }
    );

    // Initialize I2S INMP441
    if (!sampler.begin(I2S_PORT, I2S_SCK_PIN, I2S_WS_PIN, I2S_SD_PIN, SAMPLE_RATE)) {
        Serial.println("[ERROR] Failed to start I2S. Halting.");
        while (1) delay(1000);
    }

    // Connect to Wi-Fi and setup mDNS
    networkMgr.begin(WIFI_SSID, WIFI_PASSWORD, MDNS_HOSTNAME);

    // Start WebSocket Server
    wsServer.begin();

    // Handle remote commands from PC Agent or Calibrator
    wsServer.setMessageCallback([](uint8_t clientNum, const String& payload) {
        Serial.printf("[WebSocket] Received from [%u]: %s\n", clientNum, payload.c_str());

        // Dynamic threshold adjustment: {"cmd":"SET_THRESHOLD","threshold":2800}
        if (payload.indexOf("SET_THRESHOLD") >= 0) {
            int idx = payload.indexOf("\"threshold\":");
            if (idx > 0) {
                float newThresh = payload.substring(idx + 12).toFloat();
                if (newThresh > 100 && newThresh < 30000) {
                    tapDetector.setThresholds(newThresh, TAP_ATTACK_SLOPE_THRESHOLD, TAP_DEBOUNCE_MS, MULTI_TAP_WINDOW_MS);
                    Serial.printf("[Config] Updated Tap Energy Threshold to: %.1f\n", newThresh);
                }
            }
        }
    });

    // Spawn dedicated high-priority audio DSP task on Core 0
    xTaskCreatePinnedToCore(
        audioTask,
        "AudioTask",
        4096,
        NULL,
        2, // High priority
        NULL,
        0  // Core 0
    );

    Serial.println("[System] Ready! Knock on desk or surface to test.");
    pulseLED();
    delay(100);
    pulseLED();
}

void loop() {
    // Maintain network and WebSocket server
    networkMgr.update();
    wsServer.loop();

    // Update tap detector state machine (checks window expiration)
    tapDetector.update();

    // Check for detected tap events from Core 0
    TapEventMessage event;
    if (xQueueReceive(tapEventQueue, &event, 0) == pdTRUE) {
        const char* patternStr = tapPatternToString(event.pattern);
        Serial.printf("\n>>> [TAP EVENT] Pattern: %s | Peak Energy: %.1f <<<\n", patternStr, event.energy);

        char jsonBuf[128];
        snprintf(jsonBuf, sizeof(jsonBuf),
                 "{\"type\":\"EVENT\",\"source\":\"SURFACE\",\"event\":\"TAP\",\"pattern\":\"%s\",\"energy\":%.1f}",
                 patternStr, event.energy);
        Serial.println(jsonBuf);

        // Flash onboard LED as physical feedback
        pulseLED();

        // Broadcast to PC Agent over WebSocket
        wsServer.broadcastEvent("TAP", patternStr, event.energy);
    }
}
