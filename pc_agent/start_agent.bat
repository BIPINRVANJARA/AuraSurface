@echo off
title OmniTouch AI - Windows PC Agent
cd /d "%~dp0"
echo ======================================================
echo    Starting OmniTouch AI Windows PC Agent
echo ======================================================
python -m pip install -q -r requirements.txt
python src\agent.py
pause
