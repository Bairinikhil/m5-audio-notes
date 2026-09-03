#include <M5CoreS3.h>

// Offline Voice Assistant — M5CoreS3
//
// This standalone sketch is the hardware/UI foundation for the next project:
// local wake-word detection followed by local voice-command recognition.
// The TinyML speech models will be added in the next step.

enum AssistantState {
    WAITING_FOR_WAKE_WORD,
    LISTENING_FOR_COMMAND,
    COMMAND_RECOGNIZED
};

AssistantState assistantState = WAITING_FOR_WAKE_WORD;
unsigned long stateStartedAt = 0;

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

void setup() {
    CoreS3.begin();
    CoreS3.Mic.begin();
    stateStartedAt = millis();
    showState("OFFLINE ASSISTANT", "Waiting for wake word...\n\nModel integration next.");
}

void loop() {
    CoreS3.update();

    // Temporary hardware test: tap the screen to cycle through the assistant states.
    // This lets us verify the M5 display and interaction before adding TinyML models.
    if (CoreS3.Touch.getCount() > 0 && CoreS3.Touch.getDetail(0).wasPressed()) {
        if (assistantState == WAITING_FOR_WAKE_WORD) {
            assistantState = LISTENING_FOR_COMMAND;
            showState("WAKE WORD DETECTED", "Listening for command...");
        } else if (assistantState == LISTENING_FOR_COMMAND) {
            assistantState = COMMAND_RECOGNIZED;
            showState("COMMAND RECEIVED", "Ready for local command model.");
        } else {
            assistantState = WAITING_FOR_WAKE_WORD;
            showState("OFFLINE ASSISTANT", "Waiting for wake word...");
        }
        stateStartedAt = millis();
    }
}
