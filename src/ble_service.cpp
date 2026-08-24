#include "ble_service.h"
#include "rgb_status.h"
#include "sensor_manager.h"
#include <esp_system.h>

BleManager bleManager;

// ── GATT Server Callbacks ───────────────────────────────────────────────────
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        Serial.printf("[BLE] Client Connected! Client ID: %d\n", desc->conn_handle);
        bleManager._deviceConnected = true;
        rgbStatus.setState(RGB_CONNECTED);
    }

    void onDisconnect(NimBLEServer* pServer) override {
        Serial.println("[BLE] Client Disconnected. Restarting Advertising...");
        bleManager._deviceConnected = false;
        rgbStatus.setState(RGB_ADVERTISING);
        NimBLEDevice::startAdvertising();
    }
};

// ── Control & Management Callbacks ──────────────────────────────────────────
class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        uint8_t cmd = (uint8_t)value[0];
        Serial.printf("[BLE] Control command received (len=%d): 0x%02X\n", (int)value.length(), cmd);

        switch (cmd) {
            case 0x01: // Tare / Zero
                sensorManager.tareAndZero();
                break;

            case 0x02: // Trigger Measurement
                sensorManager.triggerCaptureCycle();
                break;

            case 0x10: // Request Device Info & Diagnostics
                bleManager.sendDeviceInfo();
                break;

            case 0x11: // Set Device Name: 0x11 [name string]
                if (value.length() > 1) {
                    String newName = String(value.substr(1).c_str());
                    newName.trim();
                    if (newName.length() > 0 && newName.length() <= 24) {
                        bleManager.setDeviceName(newName);
                    }
                }
                break;

            case 0x12: // Set Weight Cal Factor: 0x12 [4-byte float]
                if (value.length() >= 5) {
                    float factor = 0.0f;
                    memcpy(&factor, value.data() + 1, 4);
                    if (factor > 0.0f) {
                        bleManager.setWeightCal(factor);
                    }
                }
                break;

            case 0x13: // Set Length Cal Factor: 0x13 [4-byte float]
                if (value.length() >= 5) {
                    float factor = 0.0f;
                    memcpy(&factor, value.data() + 1, 4);
                    if (factor > 0.0f) {
                        bleManager.setLengthCal(factor);
                    }
                }
                break;

            case 0x14: // Factory Reset
                bleManager.factoryReset();
                break;

            default:
                Serial.printf("[BLE] Unknown control cmd: 0x%02X\n", cmd);
                break;
        }
    }
};

// ── BLE OTA Control Callbacks ────────────────────────────────────────────────
class OtaControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (value.empty()) return;

        uint8_t otaCmd = (uint8_t)value[0];
        Serial.printf("[OTA] Control command: 0x%02X\n", otaCmd);

        if (otaCmd == 0x01 && value.length() >= 5) {
            // 0x01 [4-byte uint32 image size]: Start OTA
            uint32_t imageSize = 0;
            memcpy(&imageSize, value.data() + 1, 4);
            bool ok = bleManager.handleOtaBegin(imageSize);
            uint8_t resp[3] = { 0x01, (uint8_t)(ok ? 0x00 : 0x01), 0x00 }; // cmd, status, progress
            pCharacteristic->setValue(resp, 3);
            pCharacteristic->notify();
        } else if (otaCmd == 0x02) {
            // 0x02: End OTA & Reboot
            bool ok = bleManager.handleOtaEnd();
            uint8_t resp[3] = { 0x02, (uint8_t)(ok ? 0x00 : 0x01), 100 };
            pCharacteristic->setValue(resp, 3);
            pCharacteristic->notify();
        } else if (otaCmd == 0x03) {
            // 0x03: Abort OTA
            bleManager.handleOtaAbort();
            uint8_t resp[3] = { 0x03, 0x00, 0x00 };
            pCharacteristic->setValue(resp, 3);
            pCharacteristic->notify();
        }
    }
};

// ── BLE OTA Data Callbacks ───────────────────────────────────────────────────
class OtaDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (!value.empty()) {
            bleManager.handleOtaChunk((const uint8_t*)value.data(), value.length());
        }
    }
};

// ── BleManager Implementation ───────────────────────────────────────────────
BleManager::BleManager() 
    : _pServer(nullptr),
      _pMeasureChar(nullptr),
      _pControlChar(nullptr),
      _pOtaControlChar(nullptr),
      _pOtaDataChar(nullptr),
      _deviceConnected(false),
      _oldDeviceConnected(false),
      _sequence(0),
      _deviceName(BLE_DEVICE_NAME_DEFAULT),
      _weightCal(420.0f),
      _lengthCal(0.5f),
      _otaInProgress(false),
      _otaHandle(0),
      _otaPartition(nullptr),
      _otaTotalSize(0),
      _otaWrittenSize(0) {}

void BleManager::begin() {
    // 1. Load NVS Settings
    _prefs.begin("cgms_cfg", false);
    _deviceName = _prefs.getString("dev_name", BLE_DEVICE_NAME_DEFAULT);
    _weightCal = _prefs.getFloat("w_cal", 420.0f);
    _lengthCal = _prefs.getFloat("l_cal", 0.5f);

    Serial.printf("[BLE] Starting NimBLE with Name: '%s'...\n", _deviceName.c_str());
    NimBLEDevice::init(_deviceName.c_str());
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9 dBm transmission power
    NimBLEDevice::setSecurityAuth(false, false, false);

    // 2. Create GATT Server
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks());

    // 3. Create Device Information Service (DIS 0x180A)
    NimBLEService* pDisService = _pServer->createService((uint16_t)0x180A);
    pDisService->createCharacteristic((uint16_t)0x2A26, NIMBLE_PROPERTY::READ)->setValue(FIRMWARE_VERSION);
    pDisService->createCharacteristic((uint16_t)0x2A27, NIMBLE_PROPERTY::READ)->setValue(HARDWARE_REVISION);
    pDisService->createCharacteristic((uint16_t)0x2A29, NIMBLE_PROPERTY::READ)->setValue(MANUFACTURER_NAME);
    pDisService->createCharacteristic((uint16_t)0x2A24, NIMBLE_PROPERTY::READ)->setValue(MODEL_NUMBER);
    pDisService->createCharacteristic((uint16_t)0x2A25, NIMBLE_PROPERTY::READ)->setValue(NimBLEDevice::getAddress().toString());
    pDisService->start();

    // 4. Create Main CGMS Anthropometry Service
    NimBLEService* pService = _pServer->createService(BLE_SERVICE_UUID);

    _pMeasureChar = pService->createCharacteristic(
        BLE_CHAR_MEASURE_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    _pControlChar = pService->createCharacteristic(
        BLE_CHAR_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );
    _pControlChar->setCallbacks(new ControlCallbacks());
    pService->start();

    // 5. Create BLE OTA Service
    NimBLEService* pOtaService = _pServer->createService(BLE_OTA_SERVICE_UUID);

    _pOtaControlChar = pOtaService->createCharacteristic(
        BLE_OTA_CHAR_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY
    );
    _pOtaControlChar->setCallbacks(new OtaControlCallbacks());

    _pOtaDataChar = pOtaService->createCharacteristic(
        BLE_OTA_CHAR_DATA_UUID,
        NIMBLE_PROPERTY::WRITE_NR
    );
    _pOtaDataChar->setCallbacks(new OtaDataCallbacks());
    pOtaService->start();

    // 6. Setup Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->addServiceUUID(BLE_OTA_SERVICE_UUID);
    pAdvertising->setName(_deviceName.c_str());
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    
    NimBLEDevice::startAdvertising();
    Serial.printf("[BLE] Advertising started as '%s' (UUID: %s)\n", _deviceName.c_str(), BLE_SERVICE_UUID);
    rgbStatus.setState(RGB_ADVERTISING);
}

String BleManager::getDeviceName() {
    return _deviceName;
}

void BleManager::setDeviceName(const String& newName) {
    _deviceName = newName;
    _prefs.putString("dev_name", newName);
    Serial.printf("[BLE] Device renamed to '%s'. Updating advertising.\n", newName.c_str());
    NimBLEDevice::getAdvertising()->setName(newName.c_str());
    sendDeviceInfo();
}

float BleManager::getWeightCal() {
    return _weightCal;
}

void BleManager::setWeightCal(float factor) {
    _weightCal = factor;
    _prefs.putFloat("w_cal", factor);
    Serial.printf("[BLE] Weight calibration factor set to %.2f\n", factor);
    sendDeviceInfo();
}

float BleManager::getLengthCal() {
    return _lengthCal;
}

void BleManager::setLengthCal(float factor) {
    _lengthCal = factor;
    _prefs.putFloat("l_cal", factor);
    Serial.printf("[BLE] Length calibration factor set to %.2f\n", factor);
    sendDeviceInfo();
}

void BleManager::factoryReset() {
    _prefs.clear();
    _deviceName = BLE_DEVICE_NAME_DEFAULT;
    _weightCal = 420.0f;
    _lengthCal = 0.5f;
    Serial.println("[BLE] Factory reset executed. Settings restored to defaults.");
    sendDeviceInfo();
}

void BleManager::sendDeviceInfo() {
    if (!_deviceConnected || !_pControlChar) return;

    // Send Device Info JSON payload
    char json[256];
    snprintf(json, sizeof(json),
             "{\"name\":\"%s\",\"fw\":\"%s\",\"hw\":\"%s\",\"mac\":\"%s\",\"w_cal\":%.2f,\"l_cal\":%.2f,\"uptime\":%lu}",
             _deviceName.c_str(), FIRMWARE_VERSION, HARDWARE_REVISION,
             NimBLEDevice::getAddress().toString().c_str(),
             _weightCal, _lengthCal, millis() / 1000);

    _pControlChar->setValue((uint8_t*)json, strlen(json));
    _pControlChar->notify();
    Serial.printf("[BLE] Device Info sent: %s\n", json);
}

void BleManager::sendMeasurement(uint8_t channel, bool stable, int32_t value_raw) {
    if (!_deviceConnected || !_pMeasureChar || _otaInProgress) return;

    uint8_t frame[FRAME_LENGTH];
    packet_encode(channel, stable, value_raw, _sequence++, frame);

    _pMeasureChar->setValue(frame, FRAME_LENGTH);
    _pMeasureChar->notify();
}

// ── BLE OTA Methods ─────────────────────────────────────────────────────────
bool BleManager::handleOtaBegin(uint32_t imageSize) {
    Serial.printf("[OTA] Beginning OTA update (size = %u bytes)...\n", imageSize);
    _otaPartition = esp_ota_get_next_update_partition(NULL);
    if (!_otaPartition) {
        Serial.println("[OTA] Error: No OTA partition found!");
        return false;
    }

    esp_err_t err = esp_ota_begin(_otaPartition, imageSize, &_otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_begin failed! Error: 0x%X\n", err);
        return false;
    }

    _otaTotalSize = imageSize;
    _otaWrittenSize = 0;
    _otaInProgress = true;
    rgbStatus.flashColor(0, 0, 255, 200); // Blue indicator
    Serial.println("[OTA] OTA partition opened successfully. Ready for chunks.");
    return true;
}

bool BleManager::handleOtaChunk(const uint8_t* data, size_t length) {
    if (!_otaInProgress) return false;

    esp_err_t err = esp_ota_write(_otaHandle, data, length);
    if (err != ESP_OK) {
        Serial.printf("[OTA] Write error at offset %u: 0x%X\n", _otaWrittenSize, err);
        handleOtaAbort();
        return false;
    }

    _otaWrittenSize += length;
    return true;
}

bool BleManager::handleOtaEnd() {
    if (!_otaInProgress) return false;

    Serial.printf("[OTA] Ending OTA (total written = %u / %u)...\n", _otaWrittenSize, _otaTotalSize);
    esp_err_t err = esp_ota_end(_otaHandle);
    if (err != ESP_OK) {
        Serial.printf("[OTA] esp_ota_end failed! Error: 0x%X\n", err);
        handleOtaAbort();
        return false;
    }

    err = esp_ota_set_boot_partition(_otaPartition);
    if (err != ESP_OK) {
        Serial.printf("[OTA] Set boot partition failed! Error: 0x%X\n", err);
        handleOtaAbort();
        return false;
    }

    Serial.println("[OTA] OTA Complete! Rebooting into new firmware in 1 second...");
    _otaInProgress = false;
    rgbStatus.flashColor(0, 255, 0, 1000); // Green success flash
    
    // Asynchronously reboot
    delay(1000);
    esp_restart();
    return true;
}

void BleManager::handleOtaAbort() {
    if (_otaInProgress) {
        Serial.println("[OTA] Update aborted.");
        esp_ota_abort(_otaHandle);
        _otaInProgress = false;
        rgbStatus.flashColor(255, 0, 0, 500); // Red error flash
    }
}

void BleManager::update() {
    if (!_deviceConnected && _oldDeviceConnected) {
        delay(20);
        _pServer->startAdvertising();
        _oldDeviceConnected = _deviceConnected;
    }
    if (_deviceConnected && !_oldDeviceConnected) {
        _oldDeviceConnected = _deviceConnected;
    }
}
