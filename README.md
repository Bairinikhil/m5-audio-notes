# M5 Audio Notes

Speak. Stop. Get clean notes on your M5CoreS3.

```text
M5CoreS3 -> Wi-Fi -> Groq Whisper -> Groq Notes -> M5 display
```

## Start in 3 steps

### 1. Install

```powershell
python -m pip install -r requirements.txt
```

### 2. Add your keys

```powershell
$env:GROQ_API_KEY = "your_groq_key"
$env:LANGSMITH_TRACING = "true"
$env:LANGSMITH_API_KEY = "your_langsmith_key"
$env:LANGSMITH_PROJECT = "m5-audio-notes"
```

LangSmith is optional. Remove those two LangSmith lines if you do not need traces.

### 3. Run it

```powershell
python main.py
```

Then open `firmware/sketch_aug27a/sketch_aug27a.ino` in Arduino IDE, add your Wi-Fi name/password and laptop IP, and upload it to the M5CoreS3.

Tap to record. Tap again to turn speech into notes.

See [Troubleshooting Notes](TROUBLESHOOTING.md) for the problems solved during development.

## What it does

- Captures 16 kHz mono audio from the M5CoreS3 microphone.
- Transcribes it with Groq Whisper.
- Creates accurate, focused notes with Groq GPT-OSS.
- Optionally traces the WAV, transcript, and summary in LangSmith.

Keep API keys, Wi-Fi passwords, audio files, and logs private.
