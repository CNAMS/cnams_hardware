#ifndef DISPLAY_OLED_H
#define DISPLAY_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"

// ── Screen States ─────────────────────────────────────────────────────────────
// OLED cycles through these states driven by main.cpp
enum OledScreen {
    OLED_SCREEN_SPLASH,       // Boot logo (1.5 s)
    OLED_SCREEN_IDLE,         // No BLE connection
    OLED_SCREEN_LIVE,         // BLE connected, real-time weight + height
    OLED_SCREEN_STABLE,       // Both channels stable / locked
    OLED_SCREEN_TARE,         // TARE / ZERO flash (1 s)
    OLED_SCREEN_MESSAGE,      // Generic two-line message
};

class DisplayOled {
public:
    DisplayOled();
    bool begin();

    // ── Main render: call from loop() at 10 Hz ────────────────────────────
    // weightG        : grams  (negative = no reading)
    // weightStable   : true when load cell is locked
    // heightMm       : millimetres (negative = no reading)
    // heightStable   : true when encoder is locked
    // isMeasuring    : capture cycle is active
    void render(bool       bleConnected,
                int32_t    weightG,     bool weightStable,
                int32_t    heightMm,    bool heightStable,
                bool       isMeasuring);

    // ── One-shot screens ─────────────────────────────────────────────────
    void showSplash();
    void showMessage(const char* title, const char* subtitle);

private:
    Adafruit_SSD1306 _display;
    bool   _isAvailable;
    uint8_t _animFrame;         // incremented on every render() call

    // ── Internal draw helpers ─────────────────────────────────────────────
    void _drawStatusBar(bool bleConnected);
    void _drawMeasurementRow(int32_t  value,
                             float    divisor,
                             const char* unit,
                             bool     stable,
                             uint8_t  yLabel,
                             uint8_t  yValue);
    void _drawBottomBar(bool isMeasuring, bool overallStable);
};

extern DisplayOled displayOled;

#endif // DISPLAY_OLED_H
