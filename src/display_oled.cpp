#include "display_oled.h"

DisplayOled displayOled;

DisplayOled::DisplayOled() 
    : _display(OLED_SCREEN_WIDTH, OLED_SCREEN_HEIGHT, &Wire, OLED_RESET_PIN),
      _isAvailable(false),
      _animFrame(0) {}

bool DisplayOled::begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    // Attempt initialization at standard I2C address 0x3C
    if (_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        _isAvailable = true;
        _display.clearDisplay();
        _display.setTextColor(SSD1306_WHITE);
        showSplash();
        return true;
    }

    // Try alternate address 0x3D if 0x3C didn't respond
    if (_display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        _isAvailable = true;
        _display.clearDisplay();
        _display.setTextColor(SSD1306_WHITE);
        showSplash();
        return true;
    }

    Serial.println("[OLED] Warning: SSD1306 display not detected on I2C bus. Running headless.");
    _isAvailable = false;
    return false;
}

void DisplayOled::showSplash() {
    if (!_isAvailable) return;
    _display.clearDisplay();
    
    // Brand header
    _display.setTextSize(2);
    _display.setCursor(14, 10);
    _display.print("ANKUR S3");

    _display.setTextSize(1);
    _display.setCursor(10, 36);
    _display.print("Smart Anthropometry");
    
    _display.setCursor(20, 50);
    _display.print("BLE: CGMS Scale");
    
    _display.display();
    delay(1000);
}

void DisplayOled::showMessage(const char* title, const char* subtitle) {
    if (!_isAvailable) return;
    _display.clearDisplay();
    
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("ANKUR CGMS");
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    _display.setTextSize(2);
    _display.setCursor(4, 20);
    _display.print(title);

    if (subtitle) {
        _display.setTextSize(1);
        _display.setCursor(4, 48);
        _display.print(subtitle);
    }

    _display.display();
}

void DisplayOled::render(bool bleConnected, int32_t weightG, int32_t heightMm, bool isMeasuring, bool isStable, const char* statusMsg) {
    if (!_isAvailable) return;
    _animFrame++;

    _display.clearDisplay();

    // ── 1. Top Status Bar ─────────────────────────────────────────────
    _display.setTextSize(1);
    _display.setCursor(0, 0);
    _display.print("ANKUR-S3");

    // BLE indicator on top right
    _display.setCursor(76, 0);
    if (bleConnected) {
        _display.print("[BLE:ON]");
    } else {
        _display.print("[BLE:ADV]");
    }
    _display.drawFastHLine(0, 10, 128, SSD1306_WHITE);

    // ── 2. Weight & Height Measurements ──────────────────────────────
    // Weight (Channel 0)
    _display.setCursor(0, 16);
    _display.print("WT: ");
    if (weightG >= 0) {
        _display.setTextSize(2);
        char wtBuf[16];
        snprintf(wtBuf, sizeof(wtBuf), "%.2f kg", (float)weightG / 1000.0f);
        _display.print(wtBuf);
    } else {
        _display.setTextSize(2);
        _display.print("--- kg");
    }

    // Height / Length (Channel 1)
    _display.setTextSize(1);
    _display.setCursor(0, 36);
    _display.print("HT: ");
    if (heightMm >= 0) {
        _display.setTextSize(2);
        char htBuf[16];
        snprintf(htBuf, sizeof(htBuf), "%.1f cm", (float)heightMm / 10.0f);
        _display.print(htBuf);
    } else {
        _display.setTextSize(2);
        _display.print("--- cm");
    }

    // ── 3. Bottom Status / Stability Badge ────────────────────────────
    _display.setTextSize(1);
    if (statusMsg) {
        _display.setCursor(0, 56);
        _display.print(statusMsg);
    } else if (isStable) {
        // Inverted badge for STABLE
        _display.fillRect(0, 54, 128, 10, SSD1306_WHITE);
        _display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        _display.setCursor(20, 55);
        _display.print("* STABLE / LOCKED *");
        _display.setTextColor(SSD1306_WHITE);
    } else if (isMeasuring) {
        _display.setCursor(0, 55);
        _display.print("Measuring");
        for (uint8_t i = 0; i < (_animFrame % 4); i++) {
            _display.print(".");
        }
    } else {
        _display.setCursor(0, 55);
        _display.print("Press BOOT to Capture");
    }

    _display.display();
}
