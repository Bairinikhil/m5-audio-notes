#include <M5CoreS3.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"

// Offline Voice Assistant — M5CoreS3
//
#define SAMPLE_RATE 16000
#define MIC_BCLK 34
#define MIC_WS 33
#define MIC_DATA 14
#define MIC_MCLK 0

enum CommandId { CMD_START_RECORDING, CMD_STOP_RECORDING, CMD_SHOW_STATUS, CMD_CLEAR_SCREEN };
static const sr_cmd_t commands[] = {
    {CMD_START_RECORDING, "Start recording"}, {CMD_START_RECORDING, "Begin recording"},
    {CMD_START_RECORDING, "Start"}, {CMD_START_RECORDING, "Begin"},
    {CMD_START_RECORDING, "Record"}, {CMD_START_RECORDING, "Record audio"},
    {CMD_START_RECORDING, "Start audio"},
    {CMD_STOP_RECORDING, "Stop recording"}, {CMD_STOP_RECORDING, "End recording"}, {CMD_STOP_RECORDING, "Stop"},
    {CMD_SHOW_STATUS, "Show status"}, {CMD_SHOW_STATUS, "Device status"},
    {CMD_CLEAR_SCREEN, "Clear screen"}, {CMD_CLEAR_SCREEN, "Clear display"}, {CMD_CLEAR_SCREEN, "Clear"}
};

I2SClass i2s;
bool recording = false;

enum AssistantState {
    WAITING_FOR_WAKE_WORD,
    LISTENING_FOR_COMMAND,
    COMMAND_RECOGNIZED
};

AssistantState assistantState = WAITING_FOR_WAKE_WORD;

void showState(const char* title, const char* detail) {
    CoreS3.Display.clear();
    CoreS3.Display.setCursor(0, 0);
    CoreS3.Display.setTextColor(GREEN);
    CoreS3.Display.setTextSize(2);
    CoreS3.Display.println(title);
    CoreS3.Display.setTextColor(WHITE);
    CoreS3.Display.setTextSize(1);
    CoreS3.Display.println();
    CoreS3.Display.println(detail);
}

void onSpeechEvent(sr_event_t event, int commandId, int phraseId) {
    (void)phraseId;
    if (event == SR_EVENT_WAKEWORD) {
        Serial.println("ESP-SR: wake word detected");
        showState("WAKE WORD", "Listening for command...");
        ESP_SR.setMode(SR_MODE_COMMAND);
    } else if (event == SR_EVENT_COMMAND) {
        Serial.printf("ESP-SR: command detected, id=%d phrase=%d\n", commandId, phraseId);
        if (commandId == CMD_START_RECORDING) { recording = true; showState("RECORDING", "Command recognized"); }
        else if (commandId == CMD_STOP_RECORDING) { recording = false; showState("STOPPED", "Recording stopped"); }
        else if (commandId == CMD_SHOW_STATUS) showState("STATUS", recording ? "Recording: ON" : "Recording: OFF");
        else if (commandId == CMD_CLEAR_SCREEN) showState("READY", "Waiting for wake word...");
        ESP_SR.setMode(SR_MODE_WAKEWORD);
    } else if (event == SR_EVENT_TIMEOUT) {
        Serial.println("ESP-SR: command timeout");
        showState("READY", "Waiting for wake word...");
        ESP_SR.setMode(SR_MODE_WAKEWORD);
    }
}

void setup() {
    Serial.begin(115200);
    CoreS3.begin();
    showState("STARTING", "Loading offline speech model...");
    i2s.setTimeout(1000);
    // CoreS3's built-in microphone is connected to I2S1.
    if (!i2s.setPort(I2S_NUM_1)) {
        showState("I2S ERROR", "Could not select microphone port");
        return;
    }
    i2s.setPins(MIC_BCLK, MIC_WS, -1, MIC_DATA, MIC_MCLK);
    if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                   I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT)) {
        showState("MIC ERROR", "Could not start I2S microphone");
        return;
    }
    ESP_SR.onEvent(onSpeechEvent);
    if (!ESP_SR.begin(i2s, commands, sizeof(commands) / sizeof(sr_cmd_t),
                      SR_CHANNELS_MONO, SR_MODE_WAKEWORD, "M")) {
        showState("MODEL ERROR", "ESP-SR could not start");
        return;
    }
    Serial.println("ESP-SR: ready; say Hi ESP, then a command");
    showState("READY", "Say the wake word, then a command");
}

void loop() {
    CoreS3.update();

    delay(10);
}
