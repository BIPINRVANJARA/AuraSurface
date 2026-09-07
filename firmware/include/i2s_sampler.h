#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

class I2SSampler {
public:
    I2SSampler();
    ~I2SSampler();

    bool begin(i2s_port_t port, int sckPin, int wsPin, int sdPin, int sampleRate = 16000);
    size_t read(int16_t* dest, size_t maxSamples);
    void end();

private:
    i2s_port_t _port;
    bool _initialized;
};
