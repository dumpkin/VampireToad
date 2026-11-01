#ifndef MOD_TEST_H
#define MOD_TEST_H

#include <Arduino.h>

namespace ModuleTest {
    extern bool test_active;
    void runDisplayColorPalette();
    void captureSplashTestRequest(bool keypadReady, unsigned long durationMs);
    void runKeypadTest();
    void runI2CScan();
}

#endif
