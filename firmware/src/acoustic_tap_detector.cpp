#include "acoustic_tap_detector.h"
#include <cmath>

const char* tapPatternToString(TapPattern pattern) {
    switch (pattern) {
        case TapPattern::SINGLE_TAP: return "SINGLE_TAP";
        case TapPattern::DOUBLE_TAP: return "DOUBLE_TAP";
        case TapPattern::TRIPLE_TAP: return "TRIPLE_TAP";
        case TapPattern::LONG_KNOCK: return "LONG_KNOCK";
        default: return "NONE";
    }
}

AcousticTapDetector::AcousticTapDetector()
    : _energyThreshold(2200.0f),
      _slopeThreshold(1200.0f),
      _debounceMs(75),
      _windowMs(380),
      _patternCallback(nullptr),
      _telemetryCallback(nullptr),
      _dcOffset(0),
      _smoothedEnergy(0.0f),
      _prevEnergy(0.0f),
      _lastPeakEnergy(0.0f),
      _lastTapTime(0),
      _windowStartTime(0),
      _tapCount(0),
      _inTapWindow(false),
      _lastTelemetryTime(0) {}

void AcousticTapDetector::setThresholds(float energyThreshold, float slopeThreshold, uint32_t debounceMs, uint32_t windowMs) {
    _energyThreshold = energyThreshold;
    _slopeThreshold = slopeThreshold;
    _debounceMs = debounceMs;
    _windowMs = windowMs;
}

void AcousticTapDetector::setCallbacks(PatternCallback patternCb, TelemetryCallback telemetryCb) {
    _patternCallback = patternCb;
    _telemetryCallback = telemetryCb;
}

void AcousticTapDetector::processSamples(const int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0) return;

    float batchPeak = 0.0f;
    float batchSum = 0.0f;

    for (size_t i = 0; i < count; i++) {
        int16_t raw = samples[i];

        // IIR High-pass / DC offset tracker
        _dcOffset = (_dcOffset * 127 + raw) / 128;
        int32_t clean = raw - _dcOffset;
        float absVal = std::abs((float)clean);

        if (absVal > batchPeak) {
            batchPeak = absVal;
        }
        batchSum += absVal;
    }

    float batchAvg = batchSum / count;

    // Envelope follower (fast attack, smooth release)
    if (batchPeak > _smoothedEnergy) {
        _smoothedEnergy = (_smoothedEnergy * 0.3f) + (batchPeak * 0.7f);
    } else {
        _smoothedEnergy = (_smoothedEnergy * 0.85f) + (batchAvg * 0.15f);
    }

    // Calculate attack slope (rate of energy rise)
    float attackSlope = _smoothedEnergy - _prevEnergy;
    _prevEnergy = _smoothedEnergy;

    uint32_t now = millis();

    // Check for impulsive physical tap
    // Must exceed energy threshold, have a sharp attack slope, and respect debounce window
    if (_smoothedEnergy >= _energyThreshold && attackSlope >= _slopeThreshold) {
        if ((now - _lastTapTime) >= _debounceMs) {
            _lastTapTime = now;
            _lastPeakEnergy = _smoothedEnergy;

            if (!_inTapWindow) {
                _inTapWindow = true;
                _windowStartTime = now;
                _tapCount = 1;
            } else {
                _tapCount++;
            }
        }
    }
}

void AcousticTapDetector::update() {
    uint32_t now = millis();

    // Check if multi-tap classification window has expired
    if (_inTapWindow && ((now - _windowStartTime) >= _windowMs)) {
        _inTapWindow = false;

        TapPattern detected = TapPattern::NONE;
        if (_tapCount == 1) {
            detected = TapPattern::SINGLE_TAP;
        } else if (_tapCount == 2) {
            detected = TapPattern::DOUBLE_TAP;
        } else if (_tapCount >= 3) {
            detected = TapPattern::TRIPLE_TAP;
        }

        if (detected != TapPattern::NONE && _patternCallback) {
            _patternCallback(detected, _lastPeakEnergy);
        }

        _tapCount = 0;
    }

    // Telemetry dispatch
    if (_telemetryCallback && (now - _lastTelemetryTime >= 50)) {
        _lastTelemetryTime = now;
        _telemetryCallback(_smoothedEnergy, _lastPeakEnergy);
    }
}
