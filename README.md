# M5 Audio Notes

An M5CoreS3 voice note device that streams microphone audio to a local Python server, transcribes it with Groq Whisper, summarizes only the explicitly stated notes with Groq GPT-OSS, and displays the result on the device.

## Flow

M5CoreS3 microphone → 16 kHz mono 16-bit PCM WebSocket chunks → FastAPI WAV assembly → Groq `whisper-large-v3-turbo` → Groq `openai/gpt-oss-20b` → summary displayed on the M5CoreS3.

## Server setup

Install dependencies:

```powershell
python -m pip install -r requirements.txt
```

Set credentials in the same PowerShell window used to start the server:

```powershell
$env:GROQ_API_KEY = "gsk_your_key"
$env:LANGSMITH_TRACING = "true"
$env:LANGSMITH_API_KEY = "lsv2_your_key"
$env:LANGSMITH_PROJECT = "m5-audio-notes"
python main.py
```

Never commit real API keys, Wi-Fi passwords, audio files, or trace logs.

## Firmware setup

Open `firmware/sketch_aug27a/sketch_aug27a.ino` in Arduino IDE. Set the local Wi-Fi name/password and the laptop's LAN IP in the sketch before uploading. The default server port is `8000`.
