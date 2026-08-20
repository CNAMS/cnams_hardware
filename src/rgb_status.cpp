#include "rgb_status.h"

RgbStatus rgbStatus;

RgbStatus::RgbStatus() 
    : _pixel(NUM_LEDS, PIN_WS2812_RGB, NEO_GRB + NEO_KHZ800),
      _currentState(RGB_BOOT),
      _lastUpdate(0),
      _step(0),
      _flashActive(false),
      _flashEndTime(0) {}

void RgbStatus::begin() {
    _pixel.begin();
    _pixel.setBrightness(40); // Gentle brightness
    _pixel.setPixelColor(0, _pixel.Color(255, 120, 0)); // Startup amber
    _pixel.show();
}

void RgbStatus::setState(RgbState state) {
    if (_currentState != state) {
        _currentState = state;
        _step = 0;
    }
}

void RgbStatus::flashColor(uint8_t r, uint8_t g, uint8_t b, uint16_t duration_ms) {
    _flashActive = true;
    _flashEndTime = millis() + duration_ms;
    _pixel.setPixelColor(0, _pixel.Color(r, g, b));
    _pixel.show();
}

void RgbStatus::update() {
    uint32_t now = millis();

    if (_flashActive) {
        if (now >= _flashEndTime) {
            _flashActive = false;
        } else {
            return; // Maintain flash
        }
    }

    if (now - _lastUpdate < 30) return; // 33 FPS update
    _lastUpdate = now;
    _step++;

    switch (_currentState) {
        case RGB_OFF:
            _pixel.setPixelColor(0, _pixel.Color(0, 0, 0));
            break;

        case RGB_BOOT: {
            uint8_t brightness = (sin(_step * 0.1) + 1.0) * 80;
            _pixel.setPixelColor(0, _pixel.Color(brightness, brightness / 2, 0));
            break;
        }

        case RGB_ADVERTISING: {
            // Pulsing blue (breathing animation)
            uint8_t b = (sin(_step * 0.08) + 1.0) * 110 + 20;
            _pixel.setPixelColor(0, _pixel.Color(0, b / 4, b));
            break;
        }

        case RGB_CONNECTED: {
            // Steady soothing green
            _pixel.setPixelColor(0, _pixel.Color(0, 180, 40));
            break;
        }

        case RGB_MEASURING: {
            // Fast yellow blink/chase
            bool blink = (_step / 4) % 2 == 0;
            if (blink) {
                _pixel.setPixelColor(0, _pixel.Color(220, 180, 0));
            } else {
                _pixel.setPixelColor(0, _pixel.Color(40, 30, 0));
            }
            break;
        }

        case RGB_LOCKED: {
            // Vivid Cyan/White solid pulse
            _pixel.setPixelColor(0, _pixel.Color(120, 255, 255));
            break;
        }

        case RGB_TARE: {
            // Red flash
            _pixel.setPixelColor(0, _pixel.Color(255, 0, 0));
            break;
        }
    }

    _pixel.show();
}
