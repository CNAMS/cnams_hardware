#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <HX711.h>
#include "config.h"

struct AnthropometryReading {
    int32_t weightG;     // Grams
    int32_t heightMm;    // Millimetres
    bool weightStable;
    bool heightStable;
    bool isOverallStable;
    bool isMeasuring;
};

class SensorManager {
public:
    SensorManager();
    void begin();
    
    // Call in main loop to process hardware and auto-capture cycles
    void update();
    
    // Triggered when user presses BOOT button (GPIO 0)
    void triggerCaptureCycle();
    
    // Triggered on long press (> 2s) for zeroing / tare
    void tareAndZero();

    // Get current reading state
    AnthropometryReading getCurrentReading();

    bool hasPhysicalSensors() const { return _physicalSensorsDetected; }

private:
    HX711 _hx711;
    bool _physicalSensorsDetected;
    bool _isMeasuring;
    uint32_t _measureStartTime;
    uint8_t _jitterStep;

    // Simulation / Target state
    int32_t _targetWeightG;
    int32_t _targetHeightMm;
    int32_t _currentWeightG;
    int32_t _currentHeightMm;
    bool _isStable;

    // Encoder variables
    static volatile int32_t _encoderTicks;
    static void IRAM_ATTR encoderIsr();
};

extern SensorManager sensorManager;

#endif // SENSOR_MANAGER_H
