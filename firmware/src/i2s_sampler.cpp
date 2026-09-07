#include "i2s_sampler.h"
#include "config.h"

I2SSampler::I2SSampler() : _port(I2S_NUM_0), _initialized(false) {}

I2SSampler::~I2SSampler() {
    end();
}

bool I2SSampler::begin(i2s_port_t port, int sckPin, int wsPin, int sdPin, int sampleRate) {
    _port = port;

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = (uint32_t)sampleRate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // INMP441 uses 32-bit frames (24-bit data)
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,   // L/R pin tied to GND
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = DMA_BUFFER_COUNT,
        .dma_buf_len = DMA_BUFFER_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = sckPin,
        .ws_io_num = wsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = sdPin
    };

    esp_err_t err = i2s_driver_install(_port, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Failed to install I2S driver: %d\n", err);
        return false;
    }

    err = i2s_set_pin(_port, &pin_config);
    if (err != ESP_OK) {
        Serial.printf("[I2S] Failed to set I2S pins: %d\n", err);
        i2s_driver_uninstall(_port);
        return false;
    }

    _initialized = true;
    Serial.println("[I2S] INMP441 I2S Sampler successfully initialized (16 kHz, 32-bit left-aligned).");
    return true;
}

size_t I2SSampler::read(int16_t* dest, size_t maxSamples) {
    if (!_initialized || dest == nullptr || maxSamples == 0) {
        return 0;
    }

    // Temporary buffer to hold raw 32-bit frames
    int32_t rawBuffer[128];
    size_t samplesReadTotal = 0;

    while (samplesReadTotal < maxSamples) {
        size_t chunkToRead = maxSamples - samplesReadTotal;
        if (chunkToRead > 128) chunkToRead = 128;

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(_port, (void*)rawBuffer, chunkToRead * sizeof(int32_t), &bytesRead, portMAX_DELAY);
        if (err != ESP_OK || bytesRead == 0) {
            break;
        }

        size_t samplesInChunk = bytesRead / sizeof(int32_t);
        for (size_t i = 0; i < samplesInChunk; i++) {
            // INMP441 outputs 24-bit data in MSB: shift by 14 or 16 to scale into signed 16-bit range
            int32_t val = rawBuffer[i] >> 14;
            if (val > 32767) val = 32767;
            if (val < -32768) val = -32768;
            dest[samplesReadTotal + i] = (int16_t)val;
        }

        samplesReadTotal += samplesInChunk;
    }

    return samplesReadTotal;
}

void I2SSampler::end() {
    if (_initialized) {
        i2s_driver_uninstall(_port);
        _initialized = false;
        Serial.println("[I2S] Driver uninstalled.");
    }
}
