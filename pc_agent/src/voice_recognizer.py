import threading
import time
from typing import Callable, Optional, Dict

class VoiceRecognizer:
    def __init__(self, command_map: Dict[str, str], on_command_callback: Callable[[str], None]):
        self.command_map = command_map
        self.on_command_callback = on_command_callback
        self.running = False
        self.thread: Optional[threading.Thread] = None

    def start_listening(self):
        """Start background speech recognition thread."""
        try:
            import speech_recognition as sr
        except ImportError:
            print("[VoiceRecognizer] Note: 'SpeechRecognition' library not installed. Voice engine disabled.")
            return

        self.running = True
        self.thread = threading.Thread(target=self._listen_loop, daemon=True)
        self.thread.start()
        print("[VoiceRecognizer] Voice Command Engine started in background.")

    def stop(self):
        self.running = False

    def _listen_loop(self):
        import speech_recognition as sr

        recognizer = sr.Recognizer()
        recognizer.energy_threshold = 1200
        recognizer.dynamic_energy_threshold = True
        recognizer.pause_threshold = 0.6

        while self.running:
            try:
                with sr.Microphone() as source:
                    print("[VoiceRecognizer] Listening for voice commands (e.g. 'mute', 'next', 'pause', 'screenshot')...")
                    audio = recognizer.listen(source, timeout=5, phrase_time_limit=4)

                # Transcribe speech
                try:
                    text = recognizer.recognize_google(audio).lower().strip()
                    print(f"[VoiceRecognizer] Heard: \"{text}\"")
                    self._match_and_dispatch(text)
                except sr.UnknownValueError:
                    # Speech was unintelligible
                    pass
                except sr.RequestError as e:
                    print(f"[VoiceRecognizer] Cloud STT service error: {e}")
                    time.sleep(2)

            except sr.WaitTimeoutError:
                continue
            except Exception as e:
                print(f"[VoiceRecognizer] Listening error: {e}")
                time.sleep(1)

    def _match_and_dispatch(self, text: str):
        """Match spoken words against command map."""
        # Exact match
        if text in self.command_map:
            action = self.command_map[text]
            print(f"[VoiceRecognizer] Matched command '{text}' -> {action}")
            self.on_command_callback(action)
            return

        # Substring / keyword match
        for key, action in self.command_map.items():
            if key in text:
                print(f"[VoiceRecognizer] Keyword matched '{key}' in '{text}' -> {action}")
                self.on_command_callback(action)
                return

    def process_pcm_chunk(self, pcm_bytes: bytes):
        """Process incoming raw PCM chunks from ESP32 INMP441 if streaming over WebSocket."""
        # Stub for streaming audio processing
        pass
