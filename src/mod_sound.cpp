#include "mod_sound.h"

namespace ModuleSound
{

    void init()
    {
        ledcSetup(BUZZER_CHAN, 2000, 8);
        ledcAttachPin(BUZZER_PIN, BUZZER_CHAN);
    }

    void play(SoundEffect effect)
    {
        switch (effect)
        {
        case SFX_CLICK:
            ledcWriteTone(BUZZER_CHAN, 3000);
            delay(10);
            ledcWrite(BUZZER_CHAN, 0);
            break;
        case SFX_OK:
            ledcWriteTone(BUZZER_CHAN, 2000);
            delay(60);
            ledcWriteTone(BUZZER_CHAN, 3500);
            delay(80);
            ledcWrite(BUZZER_CHAN, 0);
            break;
        case SFX_BACK:
            ledcWriteTone(BUZZER_CHAN, 2500);
            delay(70);
            ledcWriteTone(BUZZER_CHAN, 1500);
            delay(100);
            ledcWrite(BUZZER_CHAN, 0);
            break;
        case SFX_GPS_FIX:
            for (int i = 0; i < 3; i++)
            {
                ledcWriteTone(BUZZER_CHAN, 3200);
                delay(50);
                ledcWrite(BUZZER_CHAN, 0);
                delay(40);
            }
            break;
        case SFX_ALARM:
            for (int i = 0; i < 2; i++)
            {
                ledcWriteTone(BUZZER_CHAN, 1000);
                delay(100);
                ledcWriteTone(BUZZER_CHAN, 1800);
                delay(100);
            }
            ledcWrite(BUZZER_CHAN, 0);
            break;

        case SFX_TOAD_CROAK:
            // Серія з трьох реалістичних звуків "ква-ква-ква"
            for (int croakCount = 0; croakCount < 3; croakCount++)
            {
                // Замість падіння частоти, ми робимо серію швидких мікро-клацань.
                // Справжня жаба видає саме пачки імпульсів.
                for (int i = 0; i < 18; i++)
                {
                    // Базова низька частота Google-жаби (близько 130 Гц)
                    int baseFreq = 130;

                    // Створюємо штучний хрип (FM-модуляція)
                    // Швидко чергуємо основний тон і верхню гармоніку
                    if (i % 2 == 0)
                    {
                        ledcWriteTone(BUZZER_CHAN, baseFreq);
                    }
                    else
                    {
                        ledcWriteTone(BUZZER_CHAN, baseFreq + 50); // хрип трохи вище
                    }

                    // Дуже важливий таймінг: мікро-імпульс має бути коротким (4-6 мс)
                    delay(5);
                }

                // Апаратне вимкнення звуку між кваканнями
                ledcWriteTone(BUZZER_CHAN, 0);
                ledcWrite(BUZZER_CHAN, 0);

                // Пауза між повноцінними звуками "ква"
                delay(160);
            }
            break;
        }
    }

    // Реалізація звуку Motion Tracker (з "Чужих")
    void playRadarTick(int satsCount)
    {
        if (satsCount == 0)
        {
            // "Порожньо": глухий, низький апаратний "тік" (800 Гц)
            ledcWriteTone(BUZZER_CHAN, 800);
            delay(8);
            ledcWrite(BUZZER_CHAN, 0);
        }
        else
        {
            // "Хвилька": динамічний слайд частоти вгору залежно від супутників
            // Базова частота росте від кількості супутників (наприклад, 1200 Гц + sats * 150)
            int startFreq = 1200 + (satsCount * 120);
            if (startFreq > 4000)
                startFreq = 4000; // Обмежувач для вух

            // Мікро-слайд для ефекту "хвилі" (генерація звуку Alien Tracker)
            ledcWriteTone(BUZZER_CHAN, startFreq);
            delay(12);
            ledcWriteTone(BUZZER_CHAN, startFreq + 400); // Стрибок вгору в кінці імпульсу
            delay(8);
            ledcWrite(BUZZER_CHAN, 0);
        }
    }
}
