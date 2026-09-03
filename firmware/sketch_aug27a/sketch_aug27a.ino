#include <M5CoreS3.h>
#include <WiFi.h>
#include <WebSocketsClient.h>

// 1. CONFIGURE YOUR NETWORK (Update these)
const char* ssid     = "404 Network Not Found";
const char* password = "K@rthik2001";
const char* serverIP = "192.168.1.156"; // Set locally before uploading
const int serverPort = 8000;

WebSocketsClient webSocket;
bool isRecording = false;
bool showingSummary = false;
// Audio Configuration for the built-in microphone
#define SAMPLE_RATE 16000
// 256 samples x 2 bytes = 512 bytes of 16-bit PCM audio.
int16_t audioBuffer[256];

// Local speech gate: tune this if your room is unusually quiet or noisy.
const int32_t VAD_THRESHOLD = 400;
const uint8_t VAD_HANGOVER_CHUNKS = 30; // About 480 ms at 16 kHz.
const uint8_t VAD_PREROLL_CHUNKS = 3; // Keep about 48 ms before speech starts.
bool speechActive = false;
uint8_t silentChunks = 0;
int16_t preRoll[VAD_PREROLL_CHUNKS][256];
uint8_t preRollCount = 0;

int32_t audioLevel(const int16_t* samples, size_t count) {
    uint64_t total = 0;

    for (size_t i = 0; i < count; i++) {
        total += abs((int32_t)samples[i]);
    }

    return (int32_t)(total / count);
}

void printWrapped(const String& text) {
    const int maxChars = 26;
    String line = "";

    for (int i = 0; i < text.length(); i++) {
        char c = text[i];

        if (c == '\n' || c == '\r') {
            if (line.length() > 0) {
                CoreS3.Display.println(line);
                line = "";
            }
            continue;
        }

        line += c;

        if (line.length() >= maxChars) {
            CoreS3.Display.println(line);
            line = "";
        }
    }

    if (line.length() > 0) {
        CoreS3.Display.println(line);
    }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
    if (type == WStype_CONNECTED) {
        if (!showingSummary) {
            CoreS3.Display.clear();
            CoreS3.Display.setCursor(0, 0);
            CoreS3.Display.setTextColor(GREEN);
            CoreS3.Display.println("Server connected");
        }
    }
    else if (type == WStype_TEXT) {
        showingSummary = true;

        String summary = "";
        for (size_t i = 0; i < length; i++) {
            summary += (char)payload[i];
        }

        CoreS3.Display.clear();
        CoreS3.Display.setCursor(0, 0);

        if (summary.startsWith("ERROR:")) {
            CoreS3.Display.setTextColor(RED);
        } else {
            CoreS3.Display.setTextColor(GREEN);
        }

        CoreS3.Display.println("AI SUMMARY:");
        CoreS3.Display.setTextColor(WHITE);
        printWrapped(summary);
    }
}

void setup() {
    CoreS3.begin(); // Initializes screen, power, and hardware
    CoreS3.Display.setTextColor(GREEN);
    CoreS3.Display.setTextSize(2);
    CoreS3.Display.println("Connecting to Wi-Fi...");

    // Connect to Wi-Fi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        CoreS3.Display.print(".");
    }
    
    CoreS3.Display.println("\nWi-Fi Connected!");
    CoreS3.Display.print("IP: "); CoreS3.Display.println(WiFi.localIP());

    // Connect to your laptop's Python Backend
    webSocket.begin(serverIP, serverPort, "/process-audio");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);

    // FIX 1: Configure the microphone sample rate first using M5Stack config
    auto config = CoreS3.Mic.config();
    config.sample_rate = SAMPLE_RATE;
    CoreS3.Mic.config(config);
    
    // Begin microphone with 0 arguments as required by your library
    CoreS3.Mic.begin();
    
    CoreS3.Display.setTextColor(WHITE);
    CoreS3.Display.println("\nREADY.");
    CoreS3.Display.println("-> Tap screen to START recording");
}

void loop() {
    CoreS3.update(); // Updates hardware state
    webSocket.loop();

    // Check if user tapped the screen to toggle recording
    if (CoreS3.Touch.getCount() > 0 && CoreS3.Touch.getDetail(0).wasPressed()) {
        isRecording = !isRecording;
        if (isRecording) {
            showingSummary = false;
        }
        CoreS3.Display.clear();
        CoreS3.Display.setCursor(0, 0);

        if (isRecording) {
            CoreS3.Display.setTextColor(RED);
            CoreS3.Display.println("RECORDING AUDIO...");
            CoreS3.Display.println("Streaming to AI cloud...");
        } else {
            CoreS3.Display.setTextColor(WHITE);
            CoreS3.Display.println("RECORDING STOPPED.");
            CoreS3.Display.println("Processing AI Summary...");
            webSocket.sendTXT("END_OF_STREAM");
        }
    }

    // If recording, pull digital data from the mic and stream it instantly
    if (isRecording && webSocket.isConnected()) {
        // FIX 2: Use .record() instead of .read() as required by your library
        if (CoreS3.Mic.record(audioBuffer, 256, SAMPLE_RATE)) {
            // Gate silence locally before sending audio to the server.
            int32_t level = audioLevel(audioBuffer, 256);
            bool startedThisChunk = false;

            if (!speechActive && level >= VAD_THRESHOLD) {
                speechActive = true;
                startedThisChunk = true;
                silentChunks = 0;
                // Send the recent quiet chunks first so the first word is not clipped.
                uint8_t available = min(preRollCount, VAD_PREROLL_CHUNKS);
                uint8_t first = preRollCount >= VAD_PREROLL_CHUNKS
                    ? preRollCount % VAD_PREROLL_CHUNKS : 0;
                for (uint8_t i = 0; i < available; i++) {
                    uint8_t index = (first + i) % VAD_PREROLL_CHUNKS;
                    webSocket.sendBIN((uint8_t*)preRoll[index], sizeof(preRoll[index]));
                }
                webSocket.sendBIN((uint8_t*)audioBuffer, sizeof(audioBuffer));
            } else if (speechActive && level >= VAD_THRESHOLD) {
                silentChunks = 0;
            } else if (speechActive) {
                silentChunks++;
                if (silentChunks > VAD_HANGOVER_CHUNKS) {
                    speechActive = false;
                    preRollCount = 0;
                }
            }

            if (!speechActive) {
                memcpy(preRoll[preRollCount % VAD_PREROLL_CHUNKS], audioBuffer, sizeof(audioBuffer));
                preRollCount++;
            } else if (!startedThisChunk) {
                // Stream speech audio over the WebSocket bridge.
                webSocket.sendBIN((uint8_t*)audioBuffer, sizeof(audioBuffer));
            }
        }
    }
}
