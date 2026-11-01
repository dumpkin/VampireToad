#include "mod_buttons.h"
#include <Wire.h>
#include <SparkFunSX1509.h>

extern SX1509 io;

namespace ModuleButtons {
    namespace {
        constexpr uint8_t ROW_FIRST_PIN = 4;
        constexpr uint8_t COLUMN_FIRST_PIN = 0;
        constexpr uint8_t KEYPAD_INTERRUPT_GPIO = 4;
        constexpr uint8_t MATRIX_SIZE = 4;
        constexpr char KEY_SYMBOLS[] = "123a456b789c*0#d";
        constexpr uint8_t UP_ROW = 3;
        constexpr uint8_t UP_COLUMN = 3;
        constexpr uint8_t OK_ROW = 3;
        constexpr uint8_t OK_COLUMN = 0;
        constexpr uint8_t BACK_COLUMN = 2;
        constexpr uint8_t DOWN_ROW = 2;
        constexpr uint8_t DOWN_COLUMN = 3;
        constexpr unsigned long STABLE_INTERVAL_MS = 20;
        constexpr unsigned long SAMPLE_INTERVAL_MS = 5;
        constexpr unsigned long HOLD_INTERVAL_MS = 1200;
        constexpr size_t EVENT_QUEUE_SIZE = 8;

        Event eventQueue[EVENT_QUEUE_SIZE];
        size_t queueHead = 0;
        size_t queueTail = 0;
        size_t queueCount = 0;
        uint8_t candidateButtons = 0;
        uint8_t stableButtons = 0;
        uint8_t gestureButtons = 0;
        uint16_t candidateMatrixKeys = 0;
        uint16_t stableMatrixKeys = 0;
        unsigned long lastSampleAt = 0;
        unsigned long candidateChangedAt = 0;
        unsigned long matrixCandidateChangedAt = 0;
        unsigned long gestureStartedAt = 0;
        unsigned long ignoreUntil = 0;
        bool gestureActive = false;
        bool holdReported = false;
        bool armed = false;
        bool interruptAttached = false;
        volatile bool interruptPending = false;

        void IRAM_ATTR keypadInterruptHandler()
        {
            interruptPending = true;
        }

        void configureMatrix()
        {
            for (uint8_t row = 0; row < MATRIX_SIZE; ++row) {
                io.pinMode(ROW_FIRST_PIN + row, INPUT_PULLUP);
                io.enableInterrupt(ROW_FIRST_PIN + row, CHANGE);
            }
            for (uint8_t column = 0; column < MATRIX_SIZE; ++column) {
                io.pinMode(COLUMN_FIRST_PIN + column, OUTPUT, LOW);
            }
            io.interruptSource();
        }

        void enqueue(EventType type, uint8_t buttons, char key = '\0')
        {
            if (queueCount == EVENT_QUEUE_SIZE) {
                Serial.println("[Buttons] Event queue full; dropping event.");
                return;
            }

            eventQueue[queueTail] = {type, buttons, key};
            queueTail = (queueTail + 1) % EVENT_QUEUE_SIZE;
            ++queueCount;
        }

        uint8_t readButtons(uint16_t matrixKeys)
        {
            uint8_t buttons = 0;
            if (matrixKeys & (1U << (UP_ROW * MATRIX_SIZE + UP_COLUMN))) buttons |= UP;
            if (matrixKeys & (1U << (OK_ROW * MATRIX_SIZE + OK_COLUMN))) buttons |= OK;
            if (matrixKeys & (1U << (OK_ROW * MATRIX_SIZE + BACK_COLUMN))) buttons |= BACK;
            if (matrixKeys & (1U << (DOWN_ROW * MATRIX_SIZE + DOWN_COLUMN))) buttons |= DOWN;
            return buttons;
        }

        uint8_t readButtons()
        {
            return readButtons(ModuleButtons::readMatrix());
        }

        void updateStableButtons(unsigned long now, uint16_t sampledKeys)
        {
            const uint8_t sampledButtons = readButtons(sampledKeys);
            if (sampledButtons != candidateButtons) {
                candidateButtons = sampledButtons;
                candidateChangedAt = now;
            } else if (candidateButtons != stableButtons &&
                       now - candidateChangedAt >= STABLE_INTERVAL_MS) {
                stableButtons = candidateButtons;
            }

            if (sampledKeys != candidateMatrixKeys) {
                candidateMatrixKeys = sampledKeys;
                matrixCandidateChangedAt = now;
            } else if (candidateMatrixKeys != stableMatrixKeys &&
                       now - matrixCandidateChangedAt >= STABLE_INTERVAL_MS) {
                const uint16_t previousMatrixKeys = stableMatrixKeys;
                stableMatrixKeys = candidateMatrixKeys;
                const uint16_t newlyPressed = stableMatrixKeys & ~previousMatrixKeys;
                for (uint8_t index = 0; index < 16; ++index) {
                    if ((newlyPressed & (1U << index)) == 0) continue;
                    const char key = keySymbol(index);
                    if ((key >= '0' && key <= '9') || key == 'a' || key == 'b') {
                        enqueue(EventType::KEY, 0, key);
                    }
                }
            }
        }
    }

    uint16_t readMatrix()
    {
        const bool restoreInterrupt = interruptAttached;
        if (restoreInterrupt) detachInterrupt(digitalPinToInterrupt(KEYPAD_INTERRUPT_GPIO));
        interruptPending = false;
        io.interruptSource();

        uint16_t keys = 0;
        for (uint8_t column = 0; column < MATRIX_SIZE; ++column) {
            for (uint8_t otherColumn = 0; otherColumn < MATRIX_SIZE; ++otherColumn) {
                io.pinMode(COLUMN_FIRST_PIN + otherColumn, INPUT);
            }

            const uint8_t columnPin = COLUMN_FIRST_PIN + column;
            io.pinMode(columnPin, OUTPUT);
            io.digitalWrite(columnPin, LOW);
            delayMicroseconds(50);

            for (uint8_t row = 0; row < MATRIX_SIZE; ++row) {
                if (io.digitalRead(ROW_FIRST_PIN + row) == LOW) {
                    keys |= static_cast<uint16_t>(1U << (row * MATRIX_SIZE + column));
                }
            }

            io.pinMode(columnPin, INPUT);
        }

        for (uint8_t column = 0; column < MATRIX_SIZE; ++column) {
            io.pinMode(COLUMN_FIRST_PIN + column, OUTPUT, LOW);
        }

        io.interruptSource();
        if (restoreInterrupt) {
            attachInterrupt(digitalPinToInterrupt(KEYPAD_INTERRUPT_GPIO),
                            keypadInterruptHandler,
                            FALLING);
            if (digitalRead(KEYPAD_INTERRUPT_GPIO) == LOW) {
                interruptPending = true;
            }
        }
        return keys;
    }

    char keySymbol(uint8_t keyIndex)
    {
        return keyIndex < sizeof(KEY_SYMBOLS) - 1 ? KEY_SYMBOLS[keyIndex] : '?';
    }

    bool isKeyDown(char symbol)
    {
        for (uint8_t index = 0; index < sizeof(KEY_SYMBOLS) - 1; ++index) {
            if (KEY_SYMBOLS[index] == symbol) {
                return (readMatrix() & (1U << index)) != 0;
            }
        }
        return false;
    }

    void begin()
    {
        configureMatrix();
        pinMode(KEYPAD_INTERRUPT_GPIO, INPUT_PULLUP);
        candidateButtons = readButtons();
        stableButtons = candidateButtons;
        candidateMatrixKeys = readMatrix();
        stableMatrixKeys = candidateMatrixKeys;
        armed = stableButtons == 0;
        lastSampleAt = millis();
        candidateChangedAt = lastSampleAt;
        matrixCandidateChangedAt = lastSampleAt;
        attachInterrupt(digitalPinToInterrupt(KEYPAD_INTERRUPT_GPIO),
                        keypadInterruptHandler,
                        FALLING);
        interruptAttached = true;
    }

    void update()
    {
        const unsigned long now = millis();
        const bool debouncePending = candidateMatrixKeys != stableMatrixKeys ||
                                     candidateButtons != stableButtons;
        if ((interruptPending || debouncePending) &&
            now - lastSampleAt >= SAMPLE_INTERVAL_MS) {
            lastSampleAt = now;
            const uint16_t sampledKeys = readMatrix();
            updateStableButtons(now, sampledKeys);
        }

        if (now < ignoreUntil || !armed) {
            gestureActive = false;
            holdReported = false;
            gestureButtons = 0;
            if (stableButtons == 0) armed = true;
            return;
        }

        if (stableButtons != 0) {
            if (!gestureActive) {
                gestureActive = true;
                gestureButtons = stableButtons;
                gestureStartedAt = now;
                holdReported = false;
            } else {
                gestureButtons |= stableButtons;
            }

            if (!holdReported && now - gestureStartedAt >= HOLD_INTERVAL_MS) {
                enqueue(EventType::HOLD, gestureButtons);
                holdReported = true;
            }
            return;
        }

        if (gestureActive) {
            if (!holdReported) {
                const EventType type = (gestureButtons & (gestureButtons - 1U)) == 0
                    ? EventType::CLICK
                    : EventType::COMBINATION;
                enqueue(type, gestureButtons);
            }
            gestureActive = false;
            holdReported = false;
            gestureButtons = 0;
        }
    }

    bool popEvent(Event &event)
    {
        if (queueCount == 0) return false;

        event = eventQueue[queueHead];
        queueHead = (queueHead + 1) % EVENT_QUEUE_SIZE;
        --queueCount;
        return true;
    }

    void resetAndIgnore(unsigned long durationMs)
    {
        queueHead = 0;
        queueTail = 0;
        queueCount = 0;
        configureMatrix();

        candidateButtons = readButtons();
        stableButtons = candidateButtons;
        candidateMatrixKeys = readMatrix();
        stableMatrixKeys = candidateMatrixKeys;
        gestureButtons = 0;
        gestureActive = false;
        holdReported = false;
        armed = stableButtons == 0;
        ignoreUntil = millis() + durationMs;
        lastSampleAt = millis();
        candidateChangedAt = lastSampleAt;
        matrixCandidateChangedAt = lastSampleAt;
    }
}
