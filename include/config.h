#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// 1. PIN DEFINITIONS (ESP32-S3 N16R8)
// ============================================================================

// User Input & Status Peripherals
#define PIN_BOOT_BTN          0   // Built-in Boot Button (Active LOW)
#define PIN_WS2812_RGB        38  // Built-in WS2812B RGB LED (GPIO 38 on ESP32-S3)
#define NUM_LEDS              1   // Single onboard pixel

// I2C OLED Display (SSD1306 128x64 / 128x32)
#define PIN_I2C_SDA           17  // I2C Data Pin (can be changed to 8/21)
#define PIN_I2C_SCL           18  // I2C Clock Pin (can be changed to 9/22)
#define OLED_SCREEN_WIDTH     128
#define OLED_SCREEN_HEIGHT    64
#define OLED_RESET_PIN        -1  // Shared reset pin (-1 if not used)
#define OLED_I2C_ADDRESS      0x3C

// Physical Sensors (When wired)
#define PIN_HX711_DT          4   // HX711 Data Output Pin
#define PIN_HX711_SCK         5   // HX711 Serial Clock Pin
#define PIN_ENC_PHASE_A       6   // Quadrature Rotary Encoder Phase A
#define PIN_ENC_PHASE_B       7   // Quadrature Rotary Encoder Phase B

// ============================================================================
// Device Metadata
#define FIRMWARE_VERSION      "v1.3.0"
#define HARDWARE_REVISION     "ESP32-S3-N16R8"
#define MANUFACTURER_NAME     "Ankur CGMS"
#define MODEL_NUMBER          "ANKUR-SCALE-v1"

// Device Name advertised over BLE (Must start with "CGMS" to match cnams_app prefix)
#define BLE_DEVICE_NAME_DEFAULT "CGMS-ANKUR-S3"

// GATT 128-bit UUIDs
#define BLE_SERVICE_UUID          "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define BLE_CHAR_MEASURE_UUID     "BEB5483E-36E1-4688-B7F5-EA07361B26A8" // Notify
#define BLE_CHAR_CONTROL_UUID     "BEB5483F-36E1-4688-B7F5-EA07361B26A8" // Write/Read (Control/Tare/Cal)
#define BLE_CHAR_CONFIG_UUID      "BEB54843-36E1-4688-B7F5-EA07361B26A8" // Read/Write/Notify (Device Info & NVS Config)

// BLE OTA Service & Characteristics
#define BLE_OTA_SERVICE_UUID      "BEB54840-36E1-4688-B7F5-EA07361B26A8"
#define BLE_OTA_CHAR_CONTROL_UUID "BEB54841-36E1-4688-B7F5-EA07361B26A8" // Write/Notify (OTA Begin/End/Status)
#define BLE_OTA_CHAR_DATA_UUID    "BEB54842-36E1-4688-B7F5-EA07361B26A8" // Write Without Response (Firmware Chunks)

// ============================================================================
// 3. FROZEN PACKET PROTOCOL SPECIFICATION (11 BYTES, BIG-ENDIAN)
// ============================================================================

#define FRAME_START_MARKER    0xA5
#define FRAME_LENGTH          11

#define CHANNEL_WEIGHT        0x00  // Measured in integer grams (g)
#define CHANNEL_LENGTH        0x01  // Measured in integer millimetres (mm)

#define FLAG_STABLE           0x01
#define FLAG_UNSTABLE         0x00

// ============================================================================
// 4. MEASUREMENT & SIMULATION SETTINGS
// ============================================================================

#define BLE_STREAM_INTERVAL_MS 200  // 5 Hz notification rate
#define SIM_TICK_INTERVAL_MS   200  // 200 ms per jitter step
#define SIM_STABILIZE_TICKS    7    // Jitter cycles before stable lock
#define SIM_DEFAULT_WEIGHT_G   8540 // 8.54 kg
#define SIM_DEFAULT_LENGTH_MM  725  // 72.5 cm
#define FORCE_SIMULATION_MODE  true // Enabled for standalone ESP32 DevKit testing

#endif // CONFIG_H
