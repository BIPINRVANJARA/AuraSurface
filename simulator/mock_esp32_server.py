import asyncio
import json
import random
import sys
import threading
import time
import websockets

# ANSI colors
CYAN = "\033[96m"
GREEN = "\033[92m"
YELLOW = "\033[93m"
BOLD = "\033[1m"
RESET = "\033[0m"

connected_clients = set()
current_threshold = 2200.0


async def broadcast_event(pattern: str, energy: float):
    if not connected_clients:
        print(f"[Simulator] No clients connected to receive event: {pattern}")
        return

    msg = json.dumps({
        "type": "EVENT",
        "source": "SURFACE",
        "pattern": pattern,
        "energy": energy,
        "timestamp": int(time.time() * 1000)
    })

    print(f"{BOLD}{GREEN}>>> [BROADCASTING EVENT] {pattern} (Energy: {energy:.1f}) to {len(connected_clients)} client(s) <<<{RESET}")
    await asyncio.gather(*[ws.send(msg) for ws in connected_clients], return_exceptions=True)


async def handler(websocket):
    global current_threshold
    connected_clients.add(websocket)
    print(f"\n{BOLD}{CYAN}[Simulator] Client connected! Total clients: {len(connected_clients)}{RESET}")

    # Send welcome info
    await websocket.send(json.dumps({
        "type": "INFO",
        "device": "OmniTouch_ESP32_Simulator",
        "version": "1.0",
        "status": "SIMULATED"
    }))

    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                if data.get("cmd") == "SET_THRESHOLD":
                    current_threshold = float(data.get("threshold", 2200.0))
                    print(f"{YELLOW}[Simulator] Updated threshold to: {current_threshold}{RESET}")
            except Exception:
                pass
    finally:
        connected_clients.remove(websocket)
        print(f"{YELLOW}[Simulator] Client disconnected. Active clients: {len(connected_clients)}{RESET}")


async def telemetry_loop():
    """Stream simulated acoustic ambient levels every 60ms."""
    while True:
        if connected_clients:
            # Ambient desk vibration background noise (150 - 450)
            simulated_energy = random.uniform(150.0, 380.0)
            msg = json.dumps({
                "type": "TELEMETRY",
                "energy": round(simulated_energy, 1),
                "peak": round(simulated_energy * 1.4, 1),
                "threshold": current_threshold,
                "timestamp": int(time.time() * 1000)
            })
            await asyncio.gather(*[ws.send(msg) for ws in connected_clients], return_exceptions=True)
        await asyncio.sleep(0.06)


def interactive_cli(loop):
    print(f"\n{BOLD}{CYAN}======================================================{RESET}")
    print(f"{BOLD}{CYAN}     OmniTouch AI — Virtual ESP32 Hardware Simulator  {RESET}")
    print(f"{BOLD}{CYAN}======================================================{RESET}")
    print("Commands:")
    print("  [1] Send Single Tap  -> Triggers Play/Pause on PC Agent")
    print("  [2] Send Double Tap  -> Triggers Mic Mute on PC Agent")
    print("  [3] Send Triple Tap  -> Triggers Screenshot on PC Agent")
    print("  [q] Quit simulator\n")

    while True:
        try:
            cmd = input().strip().lower()
            if cmd == "1":
                asyncio.run_coroutine_threadsafe(broadcast_event("SINGLE_TAP", 2750.0), loop)
            elif cmd == "2":
                asyncio.run_coroutine_threadsafe(broadcast_event("DOUBLE_TAP", 3450.0), loop)
            elif cmd == "3":
                asyncio.run_coroutine_threadsafe(broadcast_event("TRIPLE_TAP", 4200.0), loop)
            elif cmd == "q":
                print("Exiting simulator...")
                sys.exit(0)
        except (EOFError, KeyboardInterrupt):
            break


async def main():
    loop = asyncio.get_running_loop()
    threading.Thread(target=interactive_cli, args=(loop,), daemon=True).start()

    server = await websockets.serve(handler, "127.0.0.1", 8765)
    print(f"{GREEN}[Simulator Server] Listening on ws://127.0.0.1:8765{RESET}")

    await asyncio.gather(server.wait_closed(), telemetry_loop())


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nSimulator stopped.")
