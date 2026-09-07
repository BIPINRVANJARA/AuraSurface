@echo off
title OmniTouch AI - Surface Tap Calibrator
cd /d "%~dp0"
echo ======================================================
echo    Starting OmniTouch AI Real-Time Calibrator
echo ======================================================
python src\calibrator.py %1
pause
