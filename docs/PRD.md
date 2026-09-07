# Product Requirements Document — OmniTouch AI

**Version:** 1.0  
**Product:** OmniTouch AI  
**Category:** Spatial HCI + IoT + AI + PC Automation  
**Platform:** ESP32 + Flutter + PC Agent + Cloud  
**Primary Controller:** ESP32  
**Mobile:** Flutter Android/iOS  
**PC:** Windows Agent  
**Backend:** Cloud API / Supabase  
**Author:** Vanjara Bipin Rajubhai  

---

## 1. Product Overview

**OmniTouch AI** is a spatial human-computer interaction platform that transforms ordinary desks, walls, and open air into a programmable control interface.

The system combines:
* 🪵 Acoustic surface interaction
* 🖐️ 3D touchless gestures
* 🌀 Motion/air-mouse control
* 🔘 Capacitive touch controls
* 🎙️ Voice commands
* 🌈 RGB lighting
* ⚡ Low-voltage appliance/device control
* 🌡️ Temperature and humidity monitoring
* 💻 PC automation
* 📱 Flutter-based configuration and monitoring

Users can interact with their computer and room without traditional buttons, keyboards, or touchscreens.

---

## 2. Problem Statement

Traditional smart-home and PC-control systems require users to interact with physical switches, smartphone apps, keyboards, mice, and remotes. These interfaces are inconvenient while working, gaming, presenting, or relaxing. OmniTouch AI provides a natural spatial interface where a simple desk tap, hand wave, or spoken word triggers actions seamlessly.

---

## 3. Product Vision

> **Turn any ordinary surface and surrounding space into an intelligent, programmable control interface.**

Users can define:
- **Gesture → Action**
- **Tap → Action**
- **Voice → Action**
- **Motion → Action**

---

## 4. Hardware BOM (Full Scope)

| Component | Purpose | Phase |
| :--- | :--- | :--- |
| **ESP32** | Main Controller & Wi-Fi/BT Hub | Phase 1 (Active) |
| **INMP441** | Acoustic Surface Tap & Voice Input | Phase 1 (Active) |
| **APDS-9960** | 3D Touchless Air Gestures | Phase 2 |
| **MPU-6050** | Motion / Tilt / Air-Mouse | Phase 2 |
| **TTP223** | Capacitive Macro Pads | Phase 2 |
| **DHT22** | Temperature & Humidity Monitoring | Phase 2 |
| **WS2812B** | Addressable RGB Status Feedback | Phase 2 |
| **4-CH Relay** | Low-Voltage Appliance Switching | Phase 2 |
| **Buzzer** | Acoustic/Haptic Feedback | Phase 2 |
