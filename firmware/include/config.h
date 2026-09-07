#pragma once

#include <Arduino.h>

// =========================================================================
// OmniTouch AI — ESP32 Firmware Configuration
// =========================================================================

// --- Wi-Fi Configuration ---
// Replace with your local Wi-Fi SSID and Password
#define WIFI_SSID           "vivoy200"
#define WIFI_PASSWORD       "77777777"

// --- Network & WebSocket Settings ---
#define MDNS_HOSTNAME       "omnitouch"       // Accessible as omnitouch.local
#define WEBSOCKET_PORT      8765              // Port for PC Agent and Calibrator
#define HTTP_PORT           80                // REST status port

// --- INMP441 I2S Microphone Pinout ---
#define I2S_SCK_PIN         33                // Serial Clock (BCLK)
#define I2S_WS_PIN          25                // Word Select (LRCK)
#define I2S_SD_PIN          32                // Serial Data (DIN / DOUT)
// Note: INMP441 L/R pin must be connected to GND (Left channel)

// --- Audio Sampling Parameters ---
#define I2S_PORT            I2S_NUM_0
#define SAMPLE_RATE         16000             // 16 kHz (standard for speech & acoustic DSP)
#define DMA_BUFFER_COUNT    4
#define DMA_BUFFER_LEN      256               // 256 samples per DMA chunk

// --- Acoustic Surface / Tap Detection Parameters ---
// Adjust TAP_ENERGY_THRESHOLD using the PC Calibrator tool
#define TAP_ENERGY_THRESHOLD        2200      // Minimum peak energy to trigger tap
#define TAP_ATTACK_SLOPE_THRESHOLD  1200      // Sharp rise required for physical tap (rejects voice)
#define TAP_DEBOUNCE_MS             75        // Lockout time to prevent surface ring re-triggering
#define MULTI_TAP_WINDOW_MS         380       // Max window to group double/triple taps (ms)

// --- Voice Command & Telemetry Parameters ---
#define VOICE_ENERGY_THRESHOLD      1500      // Energy threshold indicating ongoing speech
#define TELEMETRY_INTERVAL_MS       50        // Rate of sending acoustic levels to calibrator
#define TELEMETRY_ENABLED_DEFAULT   true      // Stream acoustic level for live calibration

// --- Onboard Status LED (optional) ---
#define STATUS_LED_PIN      2                 // Standard ESP32 onboard blue LED (GPIO 2)
