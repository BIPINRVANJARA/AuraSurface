import os
import sys
import time
import ctypes

try:
    import winsound
except ImportError:
    winsound = None

# Windows Virtual Key Codes
VK_VOLUME_MUTE = 0xAD
VK_VOLUME_DOWN = 0xAE
VK_VOLUME_UP = 0xAF
VK_MEDIA_NEXT_TRACK = 0xB0
VK_MEDIA_PREV_TRACK = 0xB1
VK_MEDIA_PLAY_PAUSE = 0xB3
VK_LWIN = 0x5B
VK_LSHIFT = 0xA0
VK_KEY_S = 0x53
VK_SNAPSHOT = 0x2C
KEYEVENTF_KEYUP = 0x0002

user32 = ctypes.windll.user32


def press_virtual_key(vk_code):
    """Simulate a single virtual key press and release using Windows user32."""
    user32.keybd_event(vk_code, 0, 0, 0)
    time.sleep(0.02)
    user32.keybd_event(vk_code, 0, KEYEVENTF_KEYUP, 0)


def trigger_snipping_tool():
    """Trigger Windows Snipping Tool (Win + Shift + S)."""
    user32.keybd_event(VK_LWIN, 0, 0, 0)
    user32.keybd_event(VK_LSHIFT, 0, 0, 0)
    user32.keybd_event(VK_KEY_S, 0, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(VK_KEY_S, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_LSHIFT, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0)


class ActionDispatcher:
    def __init__(self):
        self._mic_volume_control = None
        self._init_pycaw()

    def _init_pycaw(self):
        """Initialize Windows CoreAudio via pycaw for hardware microphone muting."""
        try:
            from pycaw.pycaw import AudioUtilities, IAudioEndpointVolume
            from comtypes import CLSCTX_ALL

            mic = AudioUtilities.GetMicrophone()
            if mic:
                interface = mic.Activate(IAudioEndpointVolume._iid_, CLSCTX_ALL, None)
                self._mic_volume_control = ctypes.cast(interface, ctypes.POINTER(IAudioEndpointVolume))
                print("[ActionDispatcher] Windows Microphone Endpoint initialized via PyCaw.")
            else:
                print("[ActionDispatcher] Default microphone endpoint not found, falling back to hotkey.")
        except Exception as e:
            print(f"[ActionDispatcher] PyCaw initialization note: {e}")
            self._mic_volume_control = None

    def toggle_mic_mute(self) -> bool:
        """Toggle system recording microphone mute status."""
        if self._mic_volume_control:
            try:
                current_mute = self._mic_volume_control.GetMute()
                new_state = not current_mute
                self._mic_volume_control.SetMute(int(new_state), None)
                status_str = "MUTED" if new_state else "UNMUTED"
                print(f"[ACTION] >>> Microphone {status_str} <<<")

                # Audio feedback beep
                if winsound:
                    freq = 400 if new_state else 800
                    winsound.Beep(freq, 120)
                return True
            except Exception as e:
                print(f"[ActionDispatcher] Error setting mic mute via pycaw: {e}")

        # Fallback: Send standard Discord / Teams mute shortcut (Ctrl+Shift+M)
        press_virtual_key(VK_VOLUME_MUTE)
        print("[ACTION] >>> Sent Audio Mute Toggle <<<")
        return True

    def play_pause(self):
        press_virtual_key(VK_MEDIA_PLAY_PAUSE)
        print("[ACTION] >>> Media Play / Pause <<<")
        if winsound:
            winsound.Beep(650, 70)

    def next_track(self):
        press_virtual_key(VK_MEDIA_NEXT_TRACK)
        print("[ACTION] >>> Next Track >>> <<<")
        if winsound:
            winsound.Beep(750, 70)

    def prev_track(self):
        press_virtual_key(VK_MEDIA_PREV_TRACK)
        print("[ACTION] >>> <<< Previous Track <<<")
        if winsound:
            winsound.Beep(550, 70)

    def volume_up(self, steps=3):
        for _ in range(steps):
            press_virtual_key(VK_VOLUME_UP)
            time.sleep(0.01)
        print(f"[ACTION] >>> Volume Up (+{steps*2}%) <<<")

    def volume_down(self, steps=3):
        for _ in range(steps):
            press_virtual_key(VK_VOLUME_DOWN)
            time.sleep(0.01)
        print(f"[ACTION] >>> Volume Down (-{steps*2}%) <<<")

    def screenshot(self):
        trigger_snipping_tool()
        print("[ACTION] >>> Snipping Tool / Screenshot Launched <<<")
        if winsound:
            winsound.Beep(900, 100)

    def execute_action(self, action_name: str) -> bool:
        """Execute action by string identifier."""
        action_name = action_name.upper().strip()
        print(f"[ActionDispatcher] Executing: {action_name}")

        if action_name in ("TOGGLE_MIC_MUTE", "MIC_MUTE", "MUTE_MIC"):
            return self.toggle_mic_mute()
        elif action_name in ("PLAY_PAUSE", "PLAY", "PAUSE"):
            self.play_pause()
            return True
        elif action_name in ("NEXT_TRACK", "NEXT"):
            self.next_track()
            return True
        elif action_name in ("PREV_TRACK", "PREVIOUS_TRACK", "PREV"):
            self.prev_track()
            return True
        elif action_name in ("VOLUME_UP", "VOL_UP"):
            self.volume_up()
            return True
        elif action_name in ("VOLUME_DOWN", "VOL_DOWN"):
            self.volume_down()
            return True
        elif action_name in ("SCREENSHOT", "SNIP"):
            self.screenshot()
            return True
        elif action_name == "MOVIE_MODE":
            print("[SCENE] Activating Movie Mode...")
            self.execute_action("PLAY_PAUSE")
            return True
        elif action_name == "STUDY_MODE":
            print("[SCENE] Activating Study Mode...")
            return True
        else:
            print(f"[ActionDispatcher] Unknown action: {action_name}")
            return False
