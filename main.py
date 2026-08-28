import os
import tempfile
import wave
from pathlib import Path
from fastapi import FastAPI, UploadFile, File, WebSocket, WebSocketDisconnect
from groq import Groq
from langsmith import traceable
from langsmith import run_trees
from langsmith.schemas import Attachment

# Compatibility fix for LangSmith 0.11.x with Pydantic forward references.
run_trees.RunTree.model_rebuild(_types_namespace={"Path": Path})

app = FastAPI(title="Ambient AI Device Backend")
SAMPLE_RATE = 16000

# Initialize Groq client from the environment, not from source code.
api_key = os.getenv("GROQ_API_KEY")
if not api_key:
    raise RuntimeError(
        "GROQ_API_KEY is not set. Set it in PowerShell before starting the server."
    )
client = Groq(api_key=api_key)


@traceable(name="Groq Whisper transcription", run_type="llm")
def transcribe_audio(audio_filename):
    with open(audio_filename, "rb") as audio_file:
        transcript_response = client.audio.transcriptions.create(
            model="whisper-large-v3-turbo",
            file=audio_file,
        )
    return transcript_response.text


@traceable(name="Groq note summary", run_type="llm")
def summarize_notes(raw_text):
    llm_response = client.chat.completions.create(
        model="openai/gpt-oss-20b",
        messages=[
            {
                "role": "system",
                "content": (
                    "You are an accurate note-taking assistant. Extract only the "
                    "meaningful points explicitly stated by the speaker: facts, "
                    "tasks, decisions, names, numbers, and deadlines. Remove filler, "
                    "repetition, and conversational noise, but do not remove needed "
                    "details. Do not invent advice, alarms, notifications, "
                    "checklists, verification steps, or follow-up actions. Do not "
                    "turn one simple task into a larger plan. Preserve the speaker's "
                    "meaning and exact times. Use as many concise bullets as needed, "
                    "one meaningful point per bullet. Output only the notes, with no "
                    "introduction or extra explanation."
                ),
            },
            {"role": "user", "content": raw_text},
        ],
        max_completion_tokens=1024,
        reasoning_effort="low",
        temperature=0.2,
    )
    return llm_response.choices[0].message.content


@traceable(name="M5CoreS3 audio note pipeline", run_type="chain")
def transcribe_and_summarize(audio_filename, wav_attachment):
    """Transcribe an audio file and turn the transcript into structured notes."""
    raw_text = transcribe_audio(audio_filename)
    ai_summary = summarize_notes(raw_text)
    return raw_text, ai_summary


def make_wav_attachment(audio_filename):
    """Create a LangSmith attachment for the exact WAV sent to transcription."""
    with open(audio_filename, "rb") as audio_file:
        return Attachment(mime_type="audio/wav", data=audio_file.read())

@app.post("/process-audio")
async def process_audio(file: UploadFile = File(...)):
    audio_filename = None
    try:
        # Save the incoming uploaded audio file.
        suffix = os.path.splitext(file.filename or "audio")[1] or ".audio"
        fd, audio_filename = tempfile.mkstemp(prefix="device_input_", suffix=suffix)
        os.close(fd)
        with open(audio_filename, "wb") as buffer:
            buffer.write(await file.read())

        raw_text, ai_summary = transcribe_and_summarize(
            audio_filename, make_wav_attachment(audio_filename)
        )

        return {
            "status": "success",
            "transcription": raw_text,
            "structured_notes": ai_summary
        }

    except Exception as e:
        return {"status": "error", "message": str(e)}
    finally:
        if audio_filename and os.path.exists(audio_filename):
            os.remove(audio_filename)


@app.websocket("/process-audio")
async def process_audio_websocket(websocket: WebSocket):
    """Receive raw 16-bit PCM chunks from the M5CoreS3 over WebSocket."""
    await websocket.accept()
    print(f"WebSocket client connected: {websocket.client}")
    audio_chunks = bytearray()

    try:
        while True:
            message = await websocket.receive()
            if message.get("bytes") is not None:
                audio_chunks.extend(message["bytes"])
            elif message.get("text") == "END_OF_STREAM":
                print(f"Audio stream ended; received {len(audio_chunks)} bytes")
                break
    except WebSocketDisconnect:
        return

    if not audio_chunks:
        print("ERROR: No audio bytes were received")
        await websocket.send_json({"status": "error", "message": "No audio received"})
        await websocket.close()
        return

    wav_filename = None
    try:
        print("Creating WAV file and sending audio to Groq...")
        fd, wav_filename = tempfile.mkstemp(prefix="device_stream_", suffix=".wav")
        os.close(fd)
        with wave.open(wav_filename, "wb") as wav_file:
            wav_file.setnchannels(1)
            wav_file.setsampwidth(2)  # 16-bit PCM from the microphone
            wav_file.setframerate(SAMPLE_RATE)
            wav_file.writeframes(audio_chunks)

        raw_text, ai_summary = transcribe_and_summarize(
            wav_filename, make_wav_attachment(wav_filename)
        )
        print(f"Audio processed successfully; summary length: {len(ai_summary or '')} characters")
        # Send plain text to the M5 so it can display the summary without JSON parsing.
        await websocket.send_text(ai_summary or "No summary was generated.")
    except Exception as e:
        print(f"ERROR while processing audio: {e}")
        await websocket.send_text(f"ERROR: {e}")
    finally:
        if wav_filename and os.path.exists(wav_filename):
            os.remove(wav_filename)
        await websocket.close()

if __name__ == "__main__":
    import uvicorn
    # Runs local server on http://localhost:8000
    uvicorn.run(app, host="0.0.0.0", port=8000)
