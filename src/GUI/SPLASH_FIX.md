# 🔧 Виправлення проблеми Сплеш-скріна

## Проблема
Сплеш-скрін при завантаженні не показувався, оскільки **замальовувався перед паузою**.

## Причина
Arduino_GFX використовує буферизацію через `Arduino_Canvas`:
- Код малює на canvas (в пам'яті)
- Canvas需要явного `flush()` для передачи на физичний дисплей
- Без `flush()` - на екрані нічого не буде видно

### Стара послідовність (баг):
```
drawSplash()
├─ gfx->fillScreen(BLACK)     → малює в canvas
├─ gfx->print("VAMPIRE TOAD") → малює в canvas
├─ ❌ НЕ РОБИТЬ flush()       ← ПРОБЛЕМА!
└─ delay(1500)                → чекаємо, але ничего не видно

drawMainMenu()                → перетирає canvas нові екран
```

## Рішення
Додано два `flush()`'s:

### 1. В `gui_common.cpp:drawSplash()`
```cpp
void drawSplash()
{
    gfx->fillScreen(BLACK);
    gfx->setTextColor(GREEN, BLACK);
    gfx->setTextSize(2);
    gfx->setCursor(12, 40);
    gfx->print("VAMPIRE TOAD");
    gfx->setTextColor(WHITE, BLACK);
    gfx->setTextSize(1);
    gfx->setCursor(44, 65);
    gfx->print("PENTA OS v2.0");

    // ✅ КРИТИЧНО: Виштовхуємо canvas на физичний дисплей
    gfx->flush();
    
    ModuleSound::play(ModuleSound::SFX_OK);
    delay(1500);
}
```

### 2. В `main.cpp:setup()`
```cpp
GUI::drawSplash();
gfx->flush();  // ✅ Гарантує сплеш на екрані перед меню
GUI::drawMainMenu();
```

## Нова послідовність (правильна):
```
drawSplash()
├─ gfx->fillScreen(BLACK)      → малює в canvas
├─ gfx->print("VAMPIRE TOAD")  → малює в canvas
├─ ✅ gfx->flush()             → ПЕРЕДАЄ на дисплей!
└─ delay(1500)                 → користувач БАЧИТЬ сплеш

(main.cpp)
├─ ✅ gfx->flush()             → підтвердження
└─ drawMainMenu()              → потім меню
```

## Результат
✅ Сплеш-скрін видимий протягом 1.5 секунди при завантаженні  
✅ Гарний перехід на головне меню  
✅ Логотип "VAMPIRE TOAD" + "PENTA OS v2.0" демонструється правильно

## Ключові моменти
- **Canvas** - буфер в пам'яті, не видимий на екрані
- **flush()** - гарантує передачу canvas на физичний дисплей
- Основний loop робить `flush()` кожні 33ms (30 FPS)
- Але при перших кадрах потрібно явне `flush()` для гарантії

## Коли ще потрібен flush()?
- Після `drawSplash()` ✅ (зроблено)
- Після критичних переходів екрану (необов'язково, але рекомендується)
- Після довгих операцій без редеревання (обычно loop() це робить)
