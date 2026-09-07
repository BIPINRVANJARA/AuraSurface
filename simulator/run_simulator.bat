@echo off
title OmniTouch AI - Virtual ESP32 Hardware Simulator
cd /d "%~dp0"
echo ======================================================
echo    Starting OmniTouch AI Virtual ESP32 Simulator
echo ======================================================
python mock_esp32_server.py
pause
