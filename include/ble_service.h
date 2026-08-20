#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "packet_codec.h"

class BleManager {
public:
    BleManager();
    void begin();
    bool isConnected() const { return _deviceConnected; }
    
    // Sends an 11-byte notification for the given channel
    void sendMeasurement(uint8_t channel, bool stable, int32_t value_raw);

    void update();

private:
    NimBLEServer* _pServer;
    NimBLECharacteristic* _pMeasureChar;
    NimBLECharacteristic* _pControlChar;
    bool _deviceConnected;
    bool _oldDeviceConnected;
    uint16_t _sequence;

    friend class ServerCallbacks;
    friend class ControlCallbacks;
};

extern BleManager bleManager;

#endif // BLE_SERVICE_H
