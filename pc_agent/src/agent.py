import asyncio
import json
import os
import sys
import time
import threading
from pathlib import Path
import websockets

from action_dispatcher import ActionDispatcher
from voice_recognizer import VoiceRecognizer
from config_manager import load_config

# ANSI Colors
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
BOLD = "\033[1m"
RESET = "\033[0m"

os.system("")


class OmniTouchPCAgent:
    def __init__(self):
        self.config = load_config()
        self.dispatcher = ActionDispatcher()

        self.esp32_host = self.config.get("esp32_host", "omnitouch.local")
        self.esp32_port = self.config.get("esp32_port", 8765)
        self.fallback_host = self.config.get("fallback_host", "127.0.0.1")
        self.tap_actions = self.config.get("haptic_tap_actions", {
            "SINGLE_TAP": "PLAY_PAUSE",
            "DOUBLE_TAP": "TOGGLE_MIC_MUTE",
            "TRIPLE_TAP": "SCREENSHOT"
        })
        self.voice_commands = self.config.get("voice_commands", {})

        # Voice recognition engine
        self.voice_recognizer = VoiceRecognizer(
            command_map=self.voice_commands,
            on_command_callback=self._handle_voice_command
        )

        # Start USB Serial background listener
        self._serial_thread = threading.Thread(target=self._serial_listener_thread, daemon=True)
        self._serial_thread.start()

    def _serial_listener_thread(self):
        """Background thread auto-connecting to ESP32 over USB Serial if available."""
        try:
            import serial
            import serial.tools.list_ports
        except ImportError:
            return

        while True:
            port = self.config.get("serial_port")
            if not port:
                ports = list(serial.tools.list_ports.comports())
                for p in ports:
                    if "CP210" in p.description or "CH340" in p.description or "USB" in p.description:
                        port = p.device
                        break

            if port:
                try:
                    with serial.Serial(port, 115200, timeout=1, dsrdtr=False, rtscts=False) as ser:
                        ser.dtr = False
                        ser.rts = False
                        print(f"{BOLD}{GREEN}[USB Serial] Connected to OmniTouch ESP32 on {port}!{RESET}")
                        while True:
                            line = ser.readline().decode("utf-8", errors="ignore").strip()
                            if not line:
                                continue
                            if '{"type":"EVENT"' in line:
                                try:
                                    start = line.find("{")
                                    end = line.rfind("}") + 1
                                    msg = json.loads(line[start:end])
                                    if msg.get("type") == "EVENT":
                                        pattern = msg.get("pattern", "UNKNOWN")
                                        energy = float(msg.get("energy", 0.0))
                                        self.handle_tap_event(pattern, energy)
                                except Exception:
                                    pass
                except Exception:
                    pass
            time.sleep(3)

    def _handle_voice_command(self, action_name: str):
        timestamp = time.strftime("%H:%M:%S")
        print(f"\n{BOLD}{CYAN}[{timestamp}] [VOICE RECOGNIZED] Executing -> {action_name}{RESET}")
        self.dispatcher.execute_action(action_name)

    def handle_tap_event(self, pattern: str, energy: float):
        timestamp = time.strftime("%H:%M:%S")
        print(f"\n{BOLD}{GREEN}[{timestamp}] [SURFACE HAPTIC EVENT] {pattern} (Energy: {energy:.1f}){RESET}")

        action = self.tap_actions.get(pattern)
        if action:
            print(f"  --> Triggering Action: {BOLD}{YELLOW}{action}{RESET}")
            self.dispatcher.execute_action(action)
        else:
            print(f"  --> No action mapped for {pattern}")

    async def connect_and_listen(self):
        hosts_to_try = [self.esp32_host, self.fallback_host]
        host_idx = 0

        while True:
            host = hosts_to_try[host_idx % len(hosts_to_try)]
            uri = f"ws://{host}:{self.esp32_port}"
            print(f"{CYAN}[Network] Connecting to OmniTouch controller at {uri} ...{RESET}")

            try:
                async with websockets.connect(uri, ping_interval=5, ping_timeout=10) as ws:
                    print(f"{BOLD}{GREEN}[Connected] OmniTouch Controller online! Ready for haptic taps & commands.{RESET}\n")

                    async for raw_message in ws:
                        try:
                            msg = json.loads(raw_message)
                            msg_type = msg.get("type")

                            if msg_type == "EVENT":
                                source = msg.get("source", "UNKNOWN")
                                pattern = msg.get("pattern", "UNKNOWN")
                                energy = float(msg.get("energy", 0.0))
                                self.handle_tap_event(pattern, energy)

                            elif msg_type == "INFO":
                                print(f"{CYAN}[Device Info] {msg.get('device')} v{msg.get('version')}{RESET}")

                        except json.JSONDecodeError:
                            pass

            except (websockets.exceptions.ConnectionClosed, OSError, asyncio.TimeoutError) as e:
                print(f"{YELLOW}[Network] Connection to {host} failed ({e}). Retrying in 3s...{RESET}")
                host_idx += 1
                await asyncio.sleep(3)

    def run(self):
        print(f"{BOLD}{CYAN}======================================================{RESET}")
        print(f"{BOLD}{CYAN}        OmniTouch AI - Windows PC Agent v1.0         {RESET}")
        print(f"{BOLD}{CYAN}   Haptic Surface Taps + Voice Commands Controller    {RESET}")
        print(f"{BOLD}{CYAN}======================================================{RESET}")
        print("Active Mappings:")
        for pattern, act in self.tap_actions.items():
            print(f"  * {pattern:12s} -> {act}")
        print("------------------------------------------------------\n")

        # Start voice command listener if enabled
        if self.voice_commands:
            self.voice_recognizer.start_listening()

        try:
            asyncio.run(self.connect_and_listen())
        except KeyboardInterrupt:
            print("\nShutting down PC Agent.")
            self.voice_recognizer.stop()


def main():
    agent = OmniTouchPCAgent()
    agent.run()


if __name__ == "__main__":
    main()
