#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <functional>

class WSServer {
public:
    using MessageCallback = std::function<void(uint8_t num, const String& payload)>;

    WSServer(uint16_t port = 8765);

    void begin();
    void loop();

    void setMessageCallback(MessageCallback cb);

    // Broadcast a detected tap/gesture event to all connected PC Agents and Flutter apps
    void broadcastEvent(const char* eventType, const char* pattern, float energy);

    // Broadcast live acoustic telemetry for real-time visual calibration
    void broadcastTelemetry(float liveEnergy, float peakEnergy, float threshold);

    // Broadcast raw audio PCM bytes (when voice mode is active)
    void broadcastAudioChunk(const uint8_t* pcmBytes, size_t length);

    size_t getConnectedClientCount() const { return _clientCount; }

private:
    WebSocketsServer _ws;
    uint16_t _port;
    size_t _clientCount;
    MessageCallback _messageCallback;

    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
};
