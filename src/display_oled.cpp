#include "display_oled.h"

DisplayOled displayOled;

// ── Pixel layout constants (128×64) ──────────────────────────────────────────
//  Row 0–9  : status bar  (device name + BLE indicator)
//  Row 10   : horizontal divider
//  Row 11–13: "WT" label  (textSize 1, 8 px tall)
//  Row 14–32: weight value (textSize 2, 16 px tall)
//  Row 33–35: "HT" label
//  Row 36–52: height value (textSize 2)
//  Row 53   : hairline divider
//  Row 54–63: bottom status bar

static constexpr uint8_t Y_DIVIDER_TOP    =  10;
static constexpr uint8_t Y_WT_LABEL       =  12;
static constexpr uint8_t Y_WT_VALUE       =  21;
static constexpr uint8_t Y_HT_LABEL       =  38;
static constexpr uint8_t Y_HT_VALUE       =  47;  // bottom of this: 47+8=55
static constexpr uint8_t Y_DIVIDER_BOT    =  54;
static constexpr uint8_t Y_STATUS_BOT     =  56;

// Column where the right-side stability glyph starts
static constexpr uint8_t X_STABLE_ICON    = 108;

// ─────────────────────────────────────────────────────────────────────────────
DisplayOled::DisplayOled()
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN),
      _isAvailable(false),
      _animFrame(0) {}

// ─────────────────────────────────────────────────────────────────────────────
bool DisplayOled::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    if (_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        _isAvailable = true;
    } else if (_display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        _isAvailable = true;
        Serial.println("[OLED] Found at alternate address 0x3D");
    } else {
        Serial.println("[OLED] Warning: SSD1306 not detected. Running headless.");
        _isAvailable = false;
        return false;
    }

    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    showSplash();
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Splash screen – shown on boot for 1.5 s
// ─────────────────────────────────────────────────────────────────────────────
void DisplayOled::showSplash() {
    if (!_isAvailable) return;
    _display.clearDisplay();

    // ── Brand name (large) ────────────────────────────────────────────────
    _display.setTextSize(2);
    _display.setCursor(10, 4);
    _display.print("ANKUR CGMS");

    // ── Divider ───────────────────────────────────────────────────────────
    _display.drawFastHLine(0, 24, 128, SSD1306_WHITE);

    // ── Subtitle ──────────────────────────────────────────────────────────
    _display.setTextSize(1);
    _display.setCursor(2, 28);
    _display.print("Smart Anthropometry");

    _display.setCursor(2, 40);
    _display.print("ESP32-S3  " FIRMWARE_VERSION);

    _display.setCursor(2, 52);
    _display.print("BLE: CGMS-ANKUR-S3");

    _display.display();
    delay(1500);
}

// ─────────────────────────────────────────────────────────────────────────────
// Generic two-line message (TARE, errors, etc.)
// ─────────────────────────────────────────────────────────────────────────────
void DisplayOled::showMessage(const char* title, const char* subtitle) {
    if (!_isAvailable) return;
    _display.clearDisplay();

    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("ANKUR CGMS");
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    _display.setTextSize(2);
    _display.setCursor(4, 18);
    _display.print(title);

    if (subtitle) {
        _display.setTextSize(1);
        _display.setCursor(4, 48);
        _display.print(subtitle);
    }

    _display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// PRIVATE HELPERS
// ─────────────────────────────────────────────────────────────────────────────

// Status bar: "CGMS-ANKUR-S3   [BLE: ✓]" or "[BLE:ADV]"
void DisplayOled::_drawStatusBar(bool bleConnected) {
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("CGMS-ANKUR-S3");

    _display.setCursor(80, 0);
    if (bleConnected) {
        _display.print("[BLE: OK]");
    } else {
        _display.print("[BLE:ADV]");
    }

    _display.drawFastHLine(0, Y_DIVIDER_TOP, 128, SSD1306_WHITE);
}

// One measurement row: label on yLabel, big value on yValue, icon at right
//   value    : raw sensor integer
//   divisor  : e.g. 1000.0f for g→kg, 10.0f for mm→cm
//   unit     : "kg" or "cm"
//   stable   : true  → show solid check indicator
//              false → show animated tilde
void DisplayOled::_drawMeasurementRow(int32_t  value,
                                      float    divisor,
                                      const char* unit,
                                      bool     stable,
                                      uint8_t  yLabel,
                                      uint8_t  yValue) {
    // ── Small label (WT / HT) ─────────────────────────────────────────────
    _display.setTextSize(1);
    _display.setCursor(0, yLabel);
    if (divisor == 1000.0f) _display.print("WT");
    else                     _display.print("HT");

    // ── Big value ─────────────────────────────────────────────────────────
    _display.setTextSize(2);
    _display.setCursor(0, yValue);
    if (value >= 0) {
        char buf[12];
        if (divisor == 1000.0f) {
            snprintf(buf, sizeof(buf), "%.2f %s", (float)value / divisor, unit);
        } else {
            snprintf(buf, sizeof(buf), "%.1f %s", (float)value / divisor, unit);
        }
        _display.print(buf);
    } else {
        _display.print("--- ");
        _display.print(unit);
    }

    // ── Stability icon (right column) ─────────────────────────────────────
    _display.setTextSize(1);
    if (stable) {
        // Solid inverted check block 10×10
        _display.fillRect(X_STABLE_ICON - 1, yValue, 20, 10, SSD1306_WHITE);
        _display.setTextColor(SSD1306_BLACK);
        _display.setCursor(X_STABLE_ICON, yValue + 1);
        _display.print("LOCK");
        _display.setTextColor(SSD1306_WHITE);
    } else if (value >= 0) {
        // Animated tilde (~~~ cycling)
        _display.setCursor(X_STABLE_ICON, yValue + 2);
        uint8_t dots = (_animFrame / 4) % 4;
        for (uint8_t i = 0; i < dots; i++) _display.print("~");
    }
}

// Bottom bar: animated measuring dots or inverted STABLE badge
void DisplayOled::_drawBottomBar(bool isMeasuring, bool overallStable) {
    _display.drawFastHLine(0, Y_DIVIDER_BOT, 128, SSD1306_WHITE);
    _display.setTextSize(1);

    if (overallStable) {
        // Full-width inverted STABLE badge
        _display.fillRect(0, Y_STATUS_BOT - 1, 128, 9, SSD1306_WHITE);
        _display.setTextColor(SSD1306_BLACK);
        _display.setCursor(14, Y_STATUS_BOT);
        _display.print("*** STABLE / LOCKED ***");
        _display.setTextColor(SSD1306_WHITE);
    } else if (isMeasuring) {
        _display.setCursor(0, Y_STATUS_BOT);
        _display.print("Measuring");
        uint8_t dots = (_animFrame / 3) % 5;
        for (uint8_t i = 0; i < dots; i++) _display.print(".");
    } else {
        _display.setCursor(0, Y_STATUS_BOT);
        _display.print("Hold BOOT=TARE");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN RENDER — called at 10 Hz from main.cpp
// ─────────────────────────────────────────────────────────────────────────────
void DisplayOled::render(bool    bleConnected,
                         int32_t weightG,    bool weightStable,
                         int32_t heightMm,   bool heightStable,
                         bool    isMeasuring) {
    if (!_isAvailable) return;
    _animFrame++;

    _display.clearDisplay();

    // ── Idle state: BLE not connected ─────────────────────────────────────
    if (!bleConnected) {
        _drawStatusBar(false);

        _display.setTextSize(1);
        _display.setCursor(0, 16);
        _display.print("WT:  --- kg");
        _display.setCursor(0, 28);
        _display.print("HT:  --- cm");

        _display.drawFastHLine(0, Y_DIVIDER_BOT, 128, SSD1306_WHITE);
        _display.setCursor(0, Y_STATUS_BOT);

        // Animated dot to show the device is alive
        _display.print("BLE scanning");
        uint8_t dot = (_animFrame / 5) % 4;
        for (uint8_t i = 0; i < dot; i++) _display.print(".");

        _display.display();
        return;
    }

    // ── Live / Stable: BLE connected ─────────────────────────────────────
    _drawStatusBar(true);

    bool overallStable = weightStable && heightStable;

    _drawMeasurementRow(weightG,  1000.0f, "kg", weightStable, Y_WT_LABEL, Y_WT_VALUE);
    _drawMeasurementRow(heightMm,   10.0f, "cm", heightStable, Y_HT_LABEL, Y_HT_VALUE);
    _drawBottomBar(isMeasuring, overallStable);

    _display.display();
}
