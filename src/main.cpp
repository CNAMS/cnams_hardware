#include <Arduino.h>
#include "config.h"
#include "rgb_status.h"
#include "display_oled.h"
#include "sensor_manager.h"
#include "ble_service.h"

// Button state tracking
static bool lastButtonState = HIGH;
static uint32_t buttonPressTime = 0;
static bool buttonLongPressTriggered = false;

// Timing intervals
static uint32_t lastBleStreamTime = 0;
static uint32_t lastDisplayRenderTime = 0;

void handleBootButton() {
    bool currentButtonState = digitalRead(PIN_BOOT_BTN);

    // Button pressed down (Active LOW)
    if (currentButtonState == LOW && lastButtonState == HIGH) {
        buttonPressTime = millis();
        buttonLongPressTriggered = false;
    }
    // Button held down
    else if (currentButtonState == LOW && lastButtonState == LOW) {
        if (!buttonLongPressTriggered && (millis() - buttonPressTime > 2000)) {
            // Long press (> 2000 ms) -> TARE / ZERO
            buttonLongPressTriggered = true;
            sensorManager.tareAndZero();
            displayOled.showMessage("TARE / ZERO", "Sensors Baseline Set");
            delay(500);
        }
    }
    // Button released
    else if (currentButtonState == HIGH && lastButtonState == LOW) {
        if (!buttonLongPressTriggered) {
            uint32_t pressDuration = millis() - buttonPressTime;
            if (pressDuration > 50) { // Debounce threshold
                // Short press -> Trigger Capture Cycle
                sensorManager.triggerCaptureCycle();
            }
        }
    }

    lastButtonState = currentButtonState;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n==================================================");
    Serial.println("  ANKUR CGMS — ESP32-S3 Smart Anthropometry Device");
    Serial.println("  Hardware: ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM)");
    Serial.println("==================================================");

    // 1. Initialize Boot Button
    pinMode(PIN_BOOT_BTN, INPUT_PULLUP);

    // 2. Initialize Status RGB LED (GPIO 38)
    rgbStatus.begin();

    // 3. Initialize OLED Display (I2C)
    displayOled.begin();

    // 4. Initialize Sensor Engine
    sensorManager.begin();

    // 5. Initialize BLE Stack & GATT Services
    bleManager.begin();

    Serial.println("[System] Initialization complete. System running.");
}

void loop() {
    uint32_t now = millis();

    // 1. Check Boot Button user input
    handleBootButton();

    // 2. Update Sensor Processing & Stability Detection
    sensorManager.update();

    // 3. Update Status LED Animation
    rgbStatus.update();

    // 4. Update BLE Service State
    bleManager.update();

    // 5. Stream BLE Readings at configured interval (5 Hz / 200 ms)
    if (now - lastBleStreamTime >= BLE_STREAM_INTERVAL_MS) {
        lastBleStreamTime = now;

        AnthropometryReading reading = sensorManager.getCurrentReading();

        if (bleManager.isConnected()) {
            // Stream Weight (Channel 0)
            bleManager.sendMeasurement(
                CHANNEL_WEIGHT,
                reading.weightStable,
                reading.weightG
            );

            // Stream Height (Channel 1)
            bleManager.sendMeasurement(
                CHANNEL_LENGTH,
                reading.heightStable,
                reading.heightMm
            );
        }
    }

    // 6. Refresh OLED Screen at 10 Hz (every 100 ms)
    if (now - lastDisplayRenderTime >= 100) {
        lastDisplayRenderTime = now;

        AnthropometryReading reading = sensorManager.getCurrentReading();
        displayOled.render(
            bleManager.isConnected(),
            reading.weightG,
            reading.heightMm,
            reading.isMeasuring,
            reading.isOverallStable
        );
    }

    delay(5); // Yield to background FreeRTOS tasks
}
