#pragma once

#include <Arduino.h>
#include <functional>

enum class TapPattern {
    NONE = 0,
    SINGLE_TAP,
    DOUBLE_TAP,
    TRIPLE_TAP,
    LONG_KNOCK
};

const char* tapPatternToString(TapPattern pattern);

class AcousticTapDetector {
public:
    using PatternCallback = std::function<void(TapPattern pattern, float peakEnergy)>;
    using TelemetryCallback = std::function<void(float liveEnergy, float peakEnergy)>;

    AcousticTapDetector();

    void setThresholds(float energyThreshold, float slopeThreshold, uint32_t debounceMs = 75, uint32_t windowMs = 380);
    void setCallbacks(PatternCallback patternCb, TelemetryCallback telemetryCb = nullptr);

    // Process a batch of 16-bit PCM audio samples from INMP441
    void processSamples(const int16_t* samples, size_t count);

    // Update state machine timers (call regularly from loop / task)
    void update();

    // Getters for live calibration
    float getLiveEnergy() const { return _smoothedEnergy; }
    float getLastPeakEnergy() const { return _lastPeakEnergy; }
    float getEnergyThreshold() const { return _energyThreshold; }

private:
    float _energyThreshold;
    float _slopeThreshold;
    uint32_t _debounceMs;
    uint32_t _windowMs;

    PatternCallback _patternCallback;
    TelemetryCallback _telemetryCallback;

    // Filter states
    int32_t _dcOffset;
    float _smoothedEnergy;
    float _prevEnergy;
    float _lastPeakEnergy;

    // Debounce & Multi-tap state
    uint32_t _lastTapTime;
    uint32_t _windowStartTime;
    int _tapCount;
    bool _inTapWindow;

    // Telemetry throttling
    uint32_t _lastTelemetryTime;
};
