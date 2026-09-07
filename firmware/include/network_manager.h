#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>

class NetworkManager {
public:
    NetworkManager();
    bool begin(const char* ssid, const char* password, const char* mdnsName = "omnitouch");
    void update();
    bool isConnected() const;
    IPAddress getIP() const;

private:
    const char* _ssid;
    const char* _password;
    const char* _mdnsName;
    uint32_t _lastReconnectAttempt;
};
