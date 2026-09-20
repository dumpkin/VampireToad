# GUI: модульна архітектура пристрою

Код інтерфейсу рознесений у `src/GUI/` по окремих модулях. Це дає чіткий поділ між логікою меню, радіо, навігацією, сенсорами, Wi‑Fi, BLE та іншими функціями пристрою.

## Що вміє пристрій
- Основне меню та перемикання між екранами
- Режим ARK та RX-MEDIA
- FM/AM прийом і ручне редагування частоти
- Регулювання гучності короткими кліками
- Компас, радар, GPS, Wi‑Fi, BLE, splash-екран
- Кнопочна логіка у стилі меню: короткий клік без довгих утримань
- Робота з I2C-датчиками, звуком, дисплеєм і мікроконтролером ESP32-S3

## Модулі пристрою
- `gui_core` — логіка меню, обробка кнопок, стани екранів
- `gui_common` — спільний layout, footer, батарея, splash
- `gui_radio` — радіо-UI, гучність, частота, редагування
- `gui_compass` — показ компасу, IMU та інерціальних даних
- `gui_radar` — GPS/радарне вікно
- `gui_wifi` — Wi‑Fi сканування та атаки
- `gui_ble` — BLE сканування та атаки
- `mod_radio` — Si4735, I2C, reset, налаштування частоти
- `mod_gy91` — IMU/GY-91, MPU9250, BMP280, AK8963
- `mod_gps` — GPS (TinyGPS++)
- `mod_sound` — бузер і звукові ефекти
- `mod_cyber` — Wi‑Fi/BLE хакерські функції та робочі модулі
- `mod_test` — відкладений тестовий блок для відладки дисплея

### Wi‑Fi модуль
- Функціонал: сканування мереж, показ SSID/RSSI/каналів, трьоетапний сценарій вибір цілі → вибір дії → виконання.
- Робочий шар: `ModuleCyber` + `gui_wifi`, використовує ESP32 Wi‑Fi API через `esp_wifi`.
- Піни: зовнішніх пінів не додає; працює через внутрішній Wi‑Fi модуль ESP32.
- Зауваження: призначений для тестів/досліджень у контролюваному середовищі.

### Bluetooth модуль
- Функціонал: сканування BLE-пристроїв, показ RSSI/імен, вибір цілі та запуск/зупинка атак/глушіння.
- Робочий шар: `ModuleCyber` + `gui_ble`, використовує вбудований BLE стек ESP32.
- Піни: без окремих GPIO; працює через внутрішній BLE контролер ESP32.
- Зауваження: використовувати лише у законному, дозволеному середовищі.

### GY‑91 модуль
- Склад: MPU9250 (акселерометр/гіроскоп), BMP280 (барометр/температура), AK8963 (магнітометр).
- I2C: SDA = GPIO8, SCL = GPIO9.
- Адреси: MPU9250 = `0x68`, BMP280 = `0x76`, AK8963 = `0x0C`.
- Функціонал: калібрування, інерціальні дані, курс, висота, швидкість, dead reckoning.
- Зауваження: AK8963 читається через I2C bypass режим MPU9250; для коректної роботи потрібен старт шини `Wire.begin()` до читання регістрів.

## Поведінка радіо
- ARK: 100–1150 kHz
- RX-MEDIA: FM 64.00–108.00 MHz, AM 520–30000 kHz
- Короткий клік по кнопках змінює гучність
- `DOWN + OK` перемикає ARK ↔ RX-MEDIA
- `UP + OK` перемикає FM ↔ AM у RX-MEDIA
- Частота редагується по цифрах

## I2C пристрої та адреси
- SX1509 expander: `0x3E`
- MPU9250: `0x68`
- BMP280: `0x76`
- AK8963: `0x0C`
- Si4735: адреса залежить від плати/підключення та перевіряється через `getDeviceI2CAddress()`

## Pinout (кодування в проекті)
- TFT (SPI): MOSI = GPIO11, SCLK = GPIO12, CS = GPIO15, DC = GPIO16, RST = GPIO14
- GY-91 (I2C): SDA = GPIO8, SCL = GPIO9
- SX1509 expander: I2C address `0x3E`
  - BTN_BACK = 1
  - BTN_OK = 2
  - BTN_DOWN = 3
  - BTN_UP = 4
  - nINT на ESP = GPIO4
- Radio reset: `RADIO_RST_GPIO = 10` або `RADIO_RST_PIN_EXPANDER = 0`
- Buzzer: `BUZZER_PIN = 2` (ledc channel 0)
- Pot / battery ADC: GPIO1

## Примітки
- `BTN_*` — це індекси пінів розширювача SX1509, а не прямі GPIO ESP32.
- `SX1509_INT_PIN` читає переривання від розширювача на GPIO4.
- `RADIO_RST_ON_EXPANDER` визначає, чи reset радіо йде через expander або GPIO ESP32.
- `src/gui.h` — основна точка входу для підключення GUI.
- Проект збирається через PlatformIO: `pio run`.

## English summary
The device is built as a modular embedded system with GUI, radio, sensor, GPS, sound, Wi‑Fi, and BLE modules. The UI is split under `src/GUI/`, and the hardware stack includes an ESP32-S3, ST7735 TFT display, SX1509 button expander, GY-91 IMU, Si4735 radio, GPS module, and buzzer. Key I2C devices are SX1509 at 0x3E, MPU9250 at 0x68, BMP280 at 0x76, and AK8963 at 0x0C; the Si4735 address is detected dynamically. Main pins: TFT SPI 11/12/15/16/14, GY-91 I2C 8/9, SX1509 INT on GPIO4, radio reset on GPIO10 or expander pin 0, buzzer on GPIO2.
