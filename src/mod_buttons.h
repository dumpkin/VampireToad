#ifndef MOD_BUTTONS_H
#define MOD_BUTTONS_H

#include <Arduino.h>

namespace ModuleButtons {
    enum Button : uint8_t {
        BACK = 1U << 0,
        OK = 1U << 1,
        UP = 1U << 2,
        DOWN = 1U << 3
    };

    enum class EventType : uint8_t {
        CLICK,
        HOLD,
        COMBINATION,
        KEY
    };

    struct Event {
        EventType type;
        uint8_t buttons;
        char key;
    };

    void begin();
    uint16_t readMatrix();
    char keySymbol(uint8_t keyIndex);
    bool isKeyDown(char symbol);
    void update();
    bool popEvent(Event &event);
    void resetAndIgnore(unsigned long durationMs);
}

#endif
