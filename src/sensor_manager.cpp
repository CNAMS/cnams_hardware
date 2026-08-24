#include "sensor_manager.h"
#include "rgb_status.h"

SensorManager sensorManager;
volatile int32_t SensorManager::_encoderTicks = 0;

void IRAM_ATTR SensorManager::encoderIsr() {
    int b = digitalRead(PIN_ENC_PHASE_B);
    if (b > 0) {
        _encoderTicks++;
    } else {
        _encoderTicks--;
    }
}

SensorManager::SensorManager() 
    : _physicalSensorsDetected(false),
      _isMeasuring(false),
      _measureStartTime(0),
      _jitterStep(0),
      _targetWeightG(SIM_DEFAULT_WEIGHT_G),
      _targetHeightMm(SIM_DEFAULT_LENGTH_MM),
      _currentWeightG(SIM_DEFAULT_WEIGHT_G),
      _currentHeightMm(SIM_DEFAULT_LENGTH_MM),
      _isStable(false) {}

void SensorManager::begin() {
#if FORCE_SIMULATION_MODE
    Serial.println("[Sensors] Force Simulation Mode enabled. Smart Anthropometry on BOOT button active.");
    _physicalSensorsDetected = false;
#else
    // 1. Initialize HX711 (if attached)
    _hx711.begin(PIN_HX711_DT, PIN_HX711_SCK);
    
    // Check if HX711 responds within 100ms
    if (_hx711.wait_ready_timeout(100)) {
        Serial.println("[Sensors] HX711 24-bit ADC detected on GPIO 4/5.");
        _hx711.set_scale(420.0f); // Default calibration factor
        _hx711.tare();
        _physicalSensorsDetected = true;
    } else {
        Serial.println("[Sensors] No physical HX711 detected. Enabling Smart Simulation on BOOT button.");
        _physicalSensorsDetected = false;
    }
#endif

    // 2. Initialize Quadrature Rotary Encoder (if attached)
    pinMode(PIN_ENC_PHASE_A, INPUT_PULLUP);
    pinMode(PIN_ENC_PHASE_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_PHASE_A), encoderIsr, RISING);

    // Default starting state
    _currentWeightG = _targetWeightG;
    _currentHeightMm = _targetHeightMm;
    _isStable = true;
}

void SensorManager::triggerCaptureCycle() {
    Serial.println("[Sensors] BOOT Button Pressed -> Starting Capture Cycle!");
    _isMeasuring = true;
    _isStable = false;
    _jitterStep = 0;
    _measureStartTime = millis();

    // Cycle through a few realistic child growth anthropometry targets for demo realism
    static uint8_t demoTargetIndex = 0;
    const int32_t demoWeights[] = { 8620, 10450, 7890, 11200, 9350 };  // grams (8.62kg, 10.45kg, ...)
    const int32_t demoHeights[] = { 728,  784,   695,  812,   750 };   // mm (72.8cm, 78.4cm, ...)
    
    demoTargetIndex = (demoTargetIndex + 1) % 5;
    _targetWeightG = demoWeights[demoTargetIndex];
    _targetHeightMm = demoHeights[demoTargetIndex];

    rgbStatus.setState(RGB_MEASURING);
}

void SensorManager::tareAndZero() {
    Serial.println("[Sensors] TARE & ZERO command executed.");
    if (_physicalSensorsDetected && _hx711.is_ready()) {
        _hx711.tare();
    }
    _encoderTicks = 0;
    _targetWeightG = 0;
    _targetHeightMm = 0;
    _currentWeightG = 0;
    _currentHeightMm = 0;
    _isStable = true;
    _isMeasuring = false;
    rgbStatus.flashColor(255, 0, 0, 500); // Red flash
}

void SensorManager::update() {
    uint32_t now = millis();

    if (_isMeasuring) {
        // Active measuring & jitter phase
        if (_jitterStep < SIM_STABILIZE_TICKS) {
            // Apply randomized jitter around target
            int32_t wJitter = (random(50) - 25) * 5;  // +/- 125g jitter
            int32_t hJitter = (random(20) - 10);      // +/- 10mm jitter
            
            _currentWeightG = _targetWeightG + wJitter;
            _currentHeightMm = _targetHeightMm + hJitter;
            _isStable = false;
            
            if (now - _measureStartTime > (uint32_t)(_jitterStep * SIM_TICK_INTERVAL_MS)) {
                _jitterStep++;
            }
        } else {
            // Settled on final locked value
            _currentWeightG = _targetWeightG;
            _currentHeightMm = _targetHeightMm;
            _isStable = true;
            _isMeasuring = false;
            rgbStatus.setState(RGB_LOCKED);
            Serial.printf("[Sensors] Measurement LOCKED: Weight = %d g, Height = %d mm\n", 
                          _currentWeightG, _currentHeightMm);
        }
    } else if (_physicalSensorsDetected && !_isStable) {
        // Only read from physical sensors when NOT in a locked/stable state.
        // This prevents floating ADC pins from overwriting simulation lock values
        // with 0 immediately after the capture cycle completes.
        // In real-hardware mode, stability tracking would reset _isStable=false
        // when the load changes, allowing continuous sensor reads again.
        if (_hx711.is_ready()) {
            float rawWeight = _hx711.get_units(2);
            _currentWeightG = (int32_t)(rawWeight * 1000.0f); // Convert kg to g
        }
        
        // Convert encoder ticks to mm (e.g. 1 tick = 0.5 mm or configured pitch)
        _currentHeightMm = (int32_t)(_encoderTicks * 0.5f);
    }
}

AnthropometryReading SensorManager::getCurrentReading() {
    AnthropometryReading reading;
    reading.weightG = _currentWeightG;
    reading.heightMm = _currentHeightMm;
    reading.weightStable = _isStable;
    reading.heightStable = _isStable;
    reading.isOverallStable = _isStable;
    reading.isMeasuring = _isMeasuring;
    return reading;
}
