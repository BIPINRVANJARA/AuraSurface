import asyncio
import json
import os
import sys
import websockets

# ANSI color codes
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
RED = "\033[91m"
BOLD = "\033[1m"
RESET = "\033[0m"

# Enable Windows virtual terminal sequences
os.system("")

def render_meter(energy: float, threshold: float, max_scale: float = 35000.0, width: int = 40) -> str:
    """Render a visual ASCII energy bar with threshold marker."""
    norm_energy = min(max(0.0, energy) / max_scale, 1.0)
    norm_thresh = min(max(0.0, threshold) / max_scale, 1.0)

    energy_pos = int(norm_energy * width)
    thresh_pos = int(norm_thresh * width)

    bar = []
    for i in range(width):
        if i == thresh_pos:
            bar.append(f"{RED}|{RESET}")
        elif i < energy_pos:
            color = GREEN if i < thresh_pos else YELLOW
            bar.append(f"{color}█{RESET}")
        else:
            bar.append("░")

    return "".join(bar)


async def run_calibrator(uri: str):
    print(f"\n{BOLD}{CYAN}======================================================{RESET}")
    print(f"{BOLD}{CYAN}   OmniTouch AI — Real-Time Acoustic Tap Calibrator   {RESET}")
    print(f"{BOLD}{CYAN}======================================================{RESET}")
    print(f"Connecting to: {uri} ...")
    print(f"Tip: System auto-profiles classroom background noise on boot.")
    print(f"Red marker '|' represents the dynamic knock trigger threshold.\n")

    current_threshold = 14500.0

    while True:
        try:
            async with websockets.connect(uri, ping_interval=5, ping_timeout=10) as ws:
                print(f"{GREEN}[Connected] Receiving live telemetry from OmniTouch ESP32!{RESET}\n")

                async for message in ws:
                    try:
                        data = json.loads(message)
                        msg_type = data.get("type")

                        if msg_type == "TELEMETRY":
                            state = data.get("state", "RUNNING")
                            energy = float(data.get("energy", 0.0))
                            floor = float(data.get("floor", 6000.0))
                            delta = float(data.get("delta", max(0.0, energy - floor)))
                            threshold = float(data.get("threshold", current_threshold))
                            current_threshold = threshold

                            bar_str = render_meter(delta, threshold, max_scale=35000.0)
                            status_color = YELLOW if delta >= threshold else CYAN
                            state_str = f"{YELLOW}[CALIBRATING]{RESET}" if state == "CALIBRATING" else f"{GREEN}[ONLINE]{RESET}"
                            sys.stdout.write(
                                f"\r{state_str} Knock Delta: [{bar_str}] "
                                f"Delta: {status_color}{int(delta):5d}{RESET} | "
                                f"Floor: {int(floor):5d} | "
                                f"Thresh: {int(threshold):5d} "
                            )
                            sys.stdout.flush()

                        elif msg_type == "CALIBRATION_START":
                            sys.stdout.write("\n")
                            print(f"{BOLD}{YELLOW}>>> [AUTO-CALIBRATION STARTED] Profiling room acoustics (3.5s)... <<<{RESET}")
                            sys.stdout.flush()

                        elif msg_type == "CALIBRATION_COMPLETE":
                            sys.stdout.write("\n")
                            floor = data.get("floor", 0)
                            spike = data.get("maxSpike", 0)
                            thresh = data.get("threshold", 0)
                            print(f"{BOLD}{GREEN}>>> [CALIBRATION COMPLETE] Floor: {floor:.0f} | Max Spike: {spike:.0f} | Dynamic Threshold: {thresh:.0f} <<<{RESET}\n")
                            sys.stdout.flush()

                        elif msg_type == "EVENT":
                            pattern = data.get("pattern", "UNKNOWN")
                            p_energy = data.get("energy", 0)
                            sys.stdout.write("\n")
                            print(f"{BOLD}{GREEN}>>> [EVENT DETECTED] {pattern} | Energy: {p_energy:.1f} <<<{RESET}")
                            sys.stdout.flush()

                    except json.JSONDecodeError:
                        pass

        except (websockets.exceptions.ConnectionClosed, OSError) as e:
            print(f"\n{YELLOW}[Disconnected] Reconnecting in 2 seconds... ({e}){RESET}")
            await asyncio.sleep(2)


def main():
    uri = "ws://10.60.64.63:8765"
    if len(sys.argv) > 1:
        uri = sys.argv[1]
    elif os.environ.get("OMNITOUCH_WS"):
        uri = os.environ.get("OMNITOUCH_WS")

    try:
        asyncio.run(run_calibrator(uri))
    except KeyboardInterrupt:
        print("\nCalibrator exited.")


if __name__ == "__main__":
    main()
