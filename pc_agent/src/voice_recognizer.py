import threading
import time
from typing import Callable, Optional, Dict
import numpy as np

class VoiceRecognizer:
    def __init__(self, command_map: Dict[str, str], on_command_callback: Callable[[str], None]):
        self.command_map = command_map
        self.on_command_callback = on_command_callback
        self.running = False
        self.thread: Optional[threading.Thread] = None

    def start_listening(self):
        """Start background speech recognition thread using sounddevice."""
        try:
            import speech_recognition as sr
            import sounddevice as sd
        except ImportError as e:
            print(f"[VoiceRecognizer] Note: Missing library ({e}). Voice engine disabled.")
            return

        self.running = True
        self.thread = threading.Thread(target=self._sounddevice_listen_loop, daemon=True)
        self.thread.start()
        print("[VoiceRecognizer] Voice Command Engine active (listening for 'mute', 'next', 'pause', etc.).")

    def stop(self):
        self.running = False

    def _sounddevice_listen_loop(self):
        import speech_recognition as sr
        import sounddevice as sd

        recognizer = sr.Recognizer()
        sample_rate = 16000
        block_duration = 0.4  # 400ms chunk
        block_size = int(sample_rate * block_duration)
        energy_threshold = 600.0  # RMS threshold for voice onset

        audio_buffer = []
        is_recording = False
        silence_blocks = 0
        max_silence_blocks = 3  # ~1.2s silence terminates speech window
        max_record_blocks = 12  # Max ~5s recording

        try:
            with sd.InputStream(samplerate=sample_rate, channels=1, dtype='int16', blocksize=block_size) as stream:
                while self.running:
                    data, _ = stream.read(block_size)
                    samples = data.flatten()
                    rms = float(np.sqrt(np.mean(samples.astype(np.float32)**2)))

                    if not is_recording:
                        if rms > energy_threshold:
                            is_recording = True
                            audio_buffer = [samples]
                            silence_blocks = 0
                    else:
                        audio_buffer.append(samples)
                        if rms < (energy_threshold * 0.75):
                            silence_blocks += 1
                        else:
                            silence_blocks = 0

                        # Terminate phrase and transcribe
                        if silence_blocks >= max_silence_blocks or len(audio_buffer) >= max_record_blocks:
                            is_recording = False
                            all_samples = np.concatenate(audio_buffer)
                            audio_bytes = all_samples.tobytes()
                            audio_data = sr.AudioData(audio_bytes, sample_rate, 2)

                            try:
                                text = recognizer.recognize_google(audio_data).lower().strip()
                                print(f"\n[VoiceRecognizer] Transcribed: \"{text}\"")
                                self._match_and_dispatch(text)
                            except sr.UnknownValueError:
                                pass
                            except Exception:
                                pass

                            audio_buffer = []
                            silence_blocks = 0
        except Exception as e:
            print(f"[VoiceRecognizer] Audio input device notice: {e}")

    def _match_and_dispatch(self, text: str):
        """Match spoken words against command map."""
        if text in self.command_map:
            action = self.command_map[text]
            print(f"[VoiceRecognizer] Matched command '{text}' -> {action}")
            self.on_command_callback(action)
            return

        for key, action in self.command_map.items():
            if key in text:
                print(f"[VoiceRecognizer] Keyword matched '{key}' in '{text}' -> {action}")
                self.on_command_callback(action)
                return
