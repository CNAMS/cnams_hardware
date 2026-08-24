#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
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

    // Device Metadata & NVS Settings
    String getDeviceName();
    void setDeviceName(const String& newName);
    float getWeightCal();
    void setWeightCal(float factor);
    float getLengthCal();
    void setLengthCal(float factor);
    void factoryReset();

    // Sends Device Info JSON payload over Control Characteristic Notify
    void sendDeviceInfo();

    // BLE OTA Handlers
    bool handleOtaBegin(uint32_t imageSize);
    bool handleOtaChunk(const uint8_t* data, size_t length);
    bool handleOtaEnd();
    void handleOtaAbort();

private:
    NimBLEServer* _pServer;
    NimBLECharacteristic* _pMeasureChar;
    NimBLECharacteristic* _pControlChar;
    NimBLECharacteristic* _pOtaControlChar;
    NimBLECharacteristic* _pOtaDataChar;

    bool _deviceConnected;
    bool _oldDeviceConnected;
    uint16_t _sequence;

    Preferences _prefs;
    String _deviceName;
    float _weightCal;
    float _lengthCal;

    // OTA State
    bool _otaInProgress;
    esp_ota_handle_t _otaHandle;
    const esp_partition_t* _otaPartition;
    uint32_t _otaTotalSize;
    uint32_t _otaWrittenSize;

    friend class ServerCallbacks;
    friend class ControlCallbacks;
    friend class OtaControlCallbacks;
    friend class OtaDataCallbacks;
};

extern BleManager bleManager;

#endif // BLE_SERVICE_H
