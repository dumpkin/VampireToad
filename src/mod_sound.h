#ifndef MOD_SOUND_H
#define MOD_SOUND_H

#include <Arduino.h>

namespace ModuleSound
{
#define BUZZER_PIN 2
#define BUZZER_CHAN 0

    enum SoundEffect
    {
        SFX_CLICK,
        SFX_OK,
        SFX_BACK,
        SFX_GPS_FIX,
        SFX_ALARM
    };

    void init();
    void play(SoundEffect effect);

    // Динамічний ефект "Чужих" для радара
    // satsCount — кількість супутників, які зараз бачить GPS
    void playRadarTick(int satsCount);
}

#endif
