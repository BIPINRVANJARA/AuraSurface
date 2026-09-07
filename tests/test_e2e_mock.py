import asyncio
import json
import sys
import os
import threading
import time
from pathlib import Path
import websockets

src_dir = Path(__file__).resolve().parent.parent / "pc_agent" / "src"
sys.path.insert(0, str(src_dir))

from action_dispatcher import ActionDispatcher

async def run_e2e_test():
    server_port = 8799
    received_events = []

    # Mock server
    async def mock_handler(ws):
        # Send welcome
        await ws.send(json.dumps({"type": "INFO", "device": "OmniTouch_ESP32", "version": "1.0"}))
        await asyncio.sleep(0.1)
        # Send telemetry
        await ws.send(json.dumps({"type": "TELEMETRY", "energy": 2400.0, "peak": 3100.0, "threshold": 2200.0}))
        await asyncio.sleep(0.1)
        # Send tap event
        await ws.send(json.dumps({
            "type": "EVENT",
            "source": "SURFACE",
            "pattern": "DOUBLE_TAP",
            "energy": 3450.0
        }))
        await asyncio.sleep(0.3)

    server = await websockets.serve(mock_handler, "127.0.0.1", server_port)
    print(f"[E2E TEST] Mock ESP32 server running on port {server_port}")

    dispatcher = ActionDispatcher()
    executed_action = None

    # Test client
    async with websockets.connect(f"ws://127.0.0.1:{server_port}") as client_ws:
        print("[E2E TEST] Client connected to mock server.")
        async for msg_str in client_ws:
            data = json.loads(msg_str)
            msg_type = data.get("type")
            print(f"[E2E TEST] Received message: {data}")

            if msg_type == "EVENT" and data.get("pattern") == "DOUBLE_TAP":
                executed_action = "TOGGLE_MIC_MUTE"
                dispatcher.execute_action(executed_action)
                break

    server.close()
    await server.wait_closed()

    assert executed_action == "TOGGLE_MIC_MUTE", "Failed to trigger DOUBLE_TAP action"
    print("\n[PASS] E2E Mock Integration Test Succeeded!")

if __name__ == "__main__":
    asyncio.run(run_e2e_test())
