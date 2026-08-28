# Troubleshooting Notes

These are the main problems we solved while building the project.

## 1. Wrong USB driver assumption

The computer had `CH343SER` installed, but the M5CoreS3 appeared as an Espressif USB device on `COM11`.

**Fix:** Use the detected `USB Serial Device (COM11)`. The M5CoreS3 does not need the CH343 driver.

## 2. Missing API credentials

The server tried to use the API key as the name of an environment variable.

**Fix:** Read the key correctly from `GROQ_API_KEY` in PowerShell.

## 3. OpenAI quota error

The ChatGPT Plus subscription did not include API credits.

**Fix:** Switched the backend to Groq’s hosted API and used its Whisper and chat models.

## 4. WebSocket `403` errors

The M5CoreS3 sent a WebSocket connection, but the server only had an HTTP upload route.

**Fix:** Added a FastAPI WebSocket route at `/process-audio`.

## 5. Incorrect transcription

The M5CoreS3 was sending 8-bit audio while the server interpreted it as 16-bit WAV audio.

**Fix:** Changed the firmware to use an `int16_t` buffer and send proper 16-bit PCM audio.

## 6. Empty summary on the device

The device expected JSON, while the server returned plain text. The firmware also cleared the screen after reconnecting.

**Fix:** Sent plain-text summaries, displayed `WStype_TEXT` responses, and preserved the summary during reconnects.

## 7. AI added unnecessary actions

The model expanded simple notes into alarms, checklists, and extra follow-up tasks.

**Fix:** Added a strict note-taking prompt that keeps only explicit facts, tasks, names, numbers, and deadlines.

## 8. LangSmith tracing error

LangSmith’s `RunTree` had a Pydantic forward-reference issue in the local Python environment.

**Fix:** Added a small compatibility rebuild before tracing starts.

## Final result

```text
M5CoreS3 microphone
  -> WebSocket PCM audio
  -> Python WAV assembly
  -> Groq Whisper transcription
  -> Groq note summary
  -> M5CoreS3 display
  -> LangSmith trace
```

