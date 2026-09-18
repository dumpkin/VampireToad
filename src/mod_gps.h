#ifndef MOD_GPS_H
#define MOD_GPS_H

#include <Arduino.h>

namespace ModuleGPS
{
    void init();
    void update();
    void stop(); // !!! Додаємо декларацію функції зупинки для лінкера
}

#endif
