#ifndef GESTURE_MANAGER_H
#define GESTURE_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"

class GestureManager {
public:
    GestureManager();
    void begin();
    void update(float accelMagnitude);  // Call every sensor cycle with accel magnitude in m/s²
    GestureType getGesture();           // Returns and CLEARS last gesture
    bool hasGesture() const;            // Check without clearing
    void setEnabled(bool enabled);      // Disable during exercise tracking
    void setHighSensitivity(bool high); // Lower thresholds for menu, higher for exercise

private:
    enum TapState { TAP_IDLE, TAP_SPIKE, TAP_QUIET, TAP_WAIT_DOUBLE };
    TapState _tapState;
    GestureType _lastGesture;
    bool _enabled;
    bool _highSensitivity;
    
    float _baseline;           // Running baseline accel magnitude
    float _tapThresholdG;      // Current tap threshold
    float _shakeThresholdG;    // Current shake threshold
    
    unsigned long _spikeStartTime;
    unsigned long _quietStartTime;
    unsigned long _firstTapTime;
    unsigned long _lastGestureTime;
    unsigned long _shakeStartTime;
    bool _shaking;
};

#endif // GESTURE_MANAGER_H
