#include "ws_server.h"

WSServer::WSServer(uint16_t port)
    : _ws(port), _port(port), _clientCount(0), _messageCallback(nullptr) {}

void WSServer::begin() {
    _ws.begin();
    _ws.onEvent([this](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        this->onWebSocketEvent(num, type, payload, length);
    });
    Serial.printf("[WebSocket] Server listening on port %u\n", _port);
}

void WSServer::loop() {
    _ws.loop();
}

void WSServer::setMessageCallback(MessageCallback cb) {
    _messageCallback = cb;
}

void WSServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            if (_clientCount > 0) _clientCount--;
            Serial.printf("[WebSocket] Client [%u] disconnected. Active clients: %u\n", num, (unsigned int)_clientCount);
            break;

        case WStype_CONNECTED: {
            _clientCount++;
            IPAddress ip = _ws.remoteIP(num);
            Serial.printf("[WebSocket] Client [%u] connected from %s. Active clients: %u\n",
                          num, ip.toString().c_str(), (unsigned int)_clientCount);

            // Send welcome greeting with device info
            char welcomeMsg[128];
            snprintf(welcomeMsg, sizeof(welcomeMsg),
                     "{\"type\":\"INFO\",\"device\":\"OmniTouch_ESP32\",\"version\":\"1.0\",\"status\":\"READY\"}");
            _ws.sendTXT(num, welcomeMsg);
            break;
        }

        case WStype_TEXT: {
            String msg = String((char*)payload).substring(0, length);
            if (_messageCallback) {
                _messageCallback(num, msg);
            }
            break;
        }

        case WStype_BIN:
            // Binary payload received if needed
            break;

        default:
            break;
    }
}

void WSServer::broadcastEvent(const char* eventType, const char* pattern, float energy) {
    if (_clientCount == 0) return;

    char buffer[160];
    snprintf(buffer, sizeof(buffer),
             "{\"type\":\"EVENT\",\"source\":\"SURFACE\",\"event\":\"%s\",\"pattern\":\"%s\",\"energy\":%.1f,\"timestamp\":%lu}",
             eventType, pattern, energy, millis());

    _ws.broadcastTXT(buffer);
}

void WSServer::broadcastTelemetry(float liveEnergy, float peakEnergy, float threshold) {
    if (_clientCount == 0) return;

    char buffer[160];
    snprintf(buffer, sizeof(buffer),
             "{\"type\":\"TELEMETRY\",\"energy\":%.1f,\"peak\":%.1f,\"threshold\":%.1f,\"timestamp\":%lu}",
             liveEnergy, peakEnergy, threshold, millis());

    _ws.broadcastTXT(buffer);
}

void WSServer::broadcastAudioChunk(const uint8_t* pcmBytes, size_t length) {
    if (_clientCount == 0 || pcmBytes == nullptr || length == 0) return;
    _ws.broadcastBIN(pcmBytes, length);
}
