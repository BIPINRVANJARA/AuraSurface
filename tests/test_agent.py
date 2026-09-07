import sys
import os
from pathlib import Path

# Add pc_agent/src to path
src_dir = Path(__file__).resolve().parent.parent / "pc_agent" / "src"
sys.path.insert(0, str(src_dir))

from action_dispatcher import ActionDispatcher
import config_manager

def test_dispatcher():
    print("[TEST] Initializing ActionDispatcher...")
    dispatcher = ActionDispatcher()
    assert dispatcher is not None, "Dispatcher failed to initialize"

    # Test executing known actions safely
    print("[TEST] Testing execution of PLAY_PAUSE...")
    assert dispatcher.execute_action("PLAY_PAUSE") == True

    print("[TEST] Testing unknown action handling...")
    assert dispatcher.execute_action("NON_EXISTENT_ACTION") == False

    print("[PASS] ActionDispatcher tests passed successfully!")

def test_config():
    print("[TEST] Testing config file loading...")
    config_path = Path(__file__).resolve().parent.parent / "pc_agent" / "config.json"
    assert config_path.exists(), "config.json does not exist"
    
    import json
    with open(config_path, "r") as f:
        data = json.load(f)
    
    assert "haptic_tap_actions" in data
    assert data["haptic_tap_actions"]["SINGLE_TAP"] == "PLAY_PAUSE"
    assert data["haptic_tap_actions"]["DOUBLE_TAP"] == "TOGGLE_MIC_MUTE"
    assert data["haptic_tap_actions"]["TRIPLE_TAP"] == "SCREENSHOT"
    print("[PASS] Config validation passed!")

if __name__ == "__main__":
    test_config()
    test_dispatcher()
    print("\n>>> ALL TESTS PASSED! <<<")
