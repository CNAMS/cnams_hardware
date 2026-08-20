#ifndef RGB_STATUS_H
#define RGB_STATUS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

enum RgbState {
    RGB_OFF,
    RGB_BOOT,          // Amber startup
    RGB_ADVERTISING,   // Pulsing Blue
    RGB_CONNECTED,     // Solid Green
    RGB_MEASURING,     // Blinking Yellow (Active jitter)
    RGB_LOCKED,        // Strobe Cyan (Reading confirmed & sent)
    RGB_TARE           // Red flash
};

class RgbStatus {
public:
    RgbStatus();
    void begin();
    void setState(RgbState state);
    void update(); // Called periodically in main loop or task
    void flashColor(uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms = 300);

private:
    Adafruit_NeoPixel _pixel;
    RgbState _currentState;
    uint32_t _lastUpdate;
    uint8_t _step;
    bool _flashActive;
    uint32_t _flashEndTime;
};

extern RgbStatus rgbStatus;

#endif // RGB_STATUS_H
