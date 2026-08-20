#include "ble_service.h"
#include "rgb_status.h"
#include "sensor_manager.h"

BleManager bleManager;

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

class ControlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.printf("[BLE] Control command received (len=%d): 0x%02X\n", 
                          value.length(), (uint8_t)value[0]);
            
            // 0x01 = Tare / Zero Command
            if ((uint8_t)value[0] == 0x01) {
                sensorManager.tareAndZero();
            }
            // 0x02 = Trigger Measurement
            else if ((uint8_t)value[0] == 0x02) {
                sensorManager.triggerCaptureCycle();
            }
        }
    }
};

BleManager::BleManager() 
    : _pServer(nullptr),
      _pMeasureChar(nullptr),
      _pControlChar(nullptr),
      _deviceConnected(false),
      _oldDeviceConnected(false),
      _sequence(0) {}

void BleManager::begin() {
    Serial.println("[BLE] Initializing NimBLE Stack...");
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); // +9 dBm transmission power
    NimBLEDevice::setSecurityAuth(false, false, false);

    // Create GATT Server
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks());

    // Create CGMS Service
    NimBLEService* pService = _pServer->createService(BLE_SERVICE_UUID);

    // 1. Measurement Characteristic (Notify)
    _pMeasureChar = pService->createCharacteristic(
        BLE_CHAR_MEASURE_UUID,
        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ
    );

    // 2. Control Characteristic (Write / Read)
    _pControlChar = pService->createCharacteristic(
        BLE_CHAR_CONTROL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ
    );
    _pControlChar->setCallbacks(new ControlCallbacks());

    // Start Service
    pService->start();

    // Setup Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setName(BLE_DEVICE_NAME);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // Functions that help with iPhone connections
    pAdvertising->setMaxPreferred(0x12);
    
    NimBLEDevice::startAdvertising();
    Serial.printf("[BLE] Advertising started as '%s' (UUID: %s)\n", BLE_DEVICE_NAME, BLE_SERVICE_UUID);
    rgbStatus.setState(RGB_ADVERTISING);
}

void BleManager::sendMeasurement(uint8_t channel, bool stable, int32_t value_raw) {
    if (!_deviceConnected || !_pMeasureChar) return;

    uint8_t frame[FRAME_LENGTH];
    packet_encode(channel, stable, value_raw, _sequence++, frame);

    _pMeasureChar->setValue(frame, FRAME_LENGTH);
    _pMeasureChar->notify();
}

void BleManager::update() {
    // Handle disconnection restart edge cases
    if (!_deviceConnected && _oldDeviceConnected) {
        delay(20);
        _pServer->startAdvertising();
        _oldDeviceConnected = _deviceConnected;
    }
    if (_deviceConnected && !_oldDeviceConnected) {
        _oldDeviceConnected = _deviceConnected;
    }
}
