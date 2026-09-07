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

def render_meter(energy: float, threshold: float, max_scale: float = 6000.0, width: int = 40) -> str:
    """Render a visual ASCII energy bar with threshold marker."""
    norm_energy = min(energy / max_scale, 1.0)
    norm_thresh = min(threshold / max_scale, 1.0)

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
    print(f"Tip: Knock or tap on your desk to see acoustic vibration peaks.")
    print(f"Red marker '|' represents the trigger threshold.\n")

    current_threshold = 2200.0

    while True:
        try:
            async with websockets.connect(uri, ping_interval=5, ping_timeout=10) as ws:
                print(f"{GREEN}[Connected] Receiving live telemetry from OmniTouch ESP32!{RESET}\n")

                async for message in ws:
                    try:
                        data = json.loads(message)
                        msg_type = data.get("type")

                        if msg_type == "TELEMETRY":
                            energy = float(data.get("energy", 0.0))
                            peak = float(data.get("peak", 0.0))
                            threshold = float(data.get("threshold", current_threshold))
                            current_threshold = threshold

                            bar_str = render_meter(energy, threshold)
                            status_color = YELLOW if energy >= threshold else CYAN
                            sys.stdout.write(
                                f"\rLevel: [{bar_str}]  "
                                f"Energy: {status_color}{int(energy):4d}{RESET} | "
                                f"Peak: {int(peak):4d} | "
                                f"Thresh: {int(threshold):4d}  "
                            )
                            sys.stdout.flush()

                        elif msg_type == "EVENT":
                            pattern = data.get("pattern", "UNKNOWN")
                            p_energy = data.get("energy", 0)
                            sys.stdout.write("\n")
                            print(f"{BOLD}{GREEN}>>> [EVENT DETECTED] {pattern} | Peak: {p_energy} <<<{RESET}")
                            sys.stdout.flush()

                    except json.JSONDecodeError:
                        pass

        except (websockets.exceptions.ConnectionClosed, OSError) as e:
            print(f"\n{YELLOW}[Disconnected] Reconnecting in 2 seconds... ({e}){RESET}")
            await asyncio.sleep(2)


def main():
    uri = "ws://127.0.0.1:8765"
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
