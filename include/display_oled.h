#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

class DisplayOled {
public:
    DisplayOled();
    bool begin();
    void render(bool bleConnected, int32_t weightG, int32_t heightMm, bool isMeasuring, bool isStable, const char* statusMsg = nullptr);
    void showSplash();
    void showMessage(const char* title, const char* subtitle);

private:
    Adafruit_SSD1306 _display;
    bool _isAvailable;
    uint8_t _animFrame;
};

extern DisplayOled displayOled;

#endif // DISPLAY_OLED_H
