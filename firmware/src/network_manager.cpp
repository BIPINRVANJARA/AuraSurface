#include "network_manager.h"

NetworkManager::NetworkManager()
    : _ssid(nullptr), _password(nullptr), _mdnsName("omnitouch"), _lastReconnectAttempt(0) {}

bool NetworkManager::begin(const char* ssid, const char* password, const char* mdnsName) {
    _ssid = ssid;
    _password = password;
    _mdnsName = mdnsName;

    Serial.printf("[WiFi] Connecting to %s ...\n", _ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);

    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 10000) {
        delay(300);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());

        if (MDNS.begin(_mdnsName)) {
            Serial.printf("[mDNS] Responder started: http://%s.local\n", _mdnsName);
            MDNS.addService("omnitouch", "tcp", 8765);
        }
        return true;
    } else {
        Serial.println("[WiFi] Connection timeout. Will retry in background.");
        return false;
    }
}

void NetworkManager::update() {
    if (WiFi.status() != WL_CONNECTED) {
        uint32_t now = millis();
        if (now - _lastReconnectAttempt > 10000) {
            _lastReconnectAttempt = now;
            Serial.println("[WiFi] Reconnecting to Wi-Fi...");
            WiFi.disconnect();
            WiFi.begin(_ssid, _password);
        }
    }
}

bool NetworkManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

IPAddress NetworkManager::getIP() const {
    return WiFi.localIP();
}
