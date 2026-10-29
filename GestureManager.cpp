#include "GestureManager.h"

GestureManager::GestureManager() {
    _tapState = TAP_IDLE;
    _lastGesture = GESTURE_NONE;
    _enabled = true;
    _highSensitivity = true;
    _baseline = GRAVITY_MS2;
    _tapThresholdG = TAP_THRESHOLD_G;
    _shakeThresholdG = SHAKE_THRESHOLD_G;
    _spikeStartTime = 0;
    _quietStartTime = 0;
    _firstTapTime = 0;
    _lastGestureTime = 0;
    _shakeStartTime = 0;
    _shaking = false;
}

void GestureManager::begin() {
    _baseline = GRAVITY_MS2;
    _tapState = TAP_IDLE;
    _lastGesture = GESTURE_NONE;
    _shaking = false;
    _enabled = true;
    setHighSensitivity(true);
}

void GestureManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (!enabled) {
        _tapState = TAP_IDLE; // Reset tap state when disabling
    }
}

void GestureManager::setHighSensitivity(bool high) {
    _highSensitivity = high;
    if (high) {
        _tapThresholdG = TAP_THRESHOLD_G;
        _shakeThresholdG = SHAKE_THRESHOLD_G;
    } else {
        _tapThresholdG = TAP_THRESHOLD_G * 1.5f; // Harder to trigger accidentally
        _shakeThresholdG = EXERCISE_SHAKE_THRESHOLD_G;
    }
}

void GestureManager::update(float accelMagnitude) {
    unsigned long now = millis();
    
    if (now - _lastGestureTime < GESTURE_COOLDOWN_MS) {
        return; // In cooldown
    }
    
    // 1. Update running baseline
    // Slow LPF: baseline = 0.99 * baseline + 0.01 * accelMag
    _baseline = 0.99f * _baseline + 0.01f * accelMagnitude;
    
    // 2. Calculate deviation in G
    float deviation = abs(accelMagnitude - _baseline) / GRAVITY_MS2;
    
    // 4. Shake detection (runs in parallel)
    if (deviation > _shakeThresholdG) {
        if (!_shaking) {
            _shaking = true;
            _shakeStartTime = now;
        } else if (now - _shakeStartTime > SHAKE_DURATION_MS) {
            DBG_GEST("Gesture: SHAKE\n");
            _lastGesture = GESTURE_SHAKE;
            _lastGestureTime = now;
            _shaking = false;
            _tapState = TAP_IDLE;
            return; // Exit early, shake takes precedence
        }
    } else {
        _shaking = false;
    }

    if (!_enabled) return; // Only process shake if disabled

    // 3. Tap detection state machine
    switch (_tapState) {
        case TAP_IDLE:
            if (deviation > _tapThresholdG) {
                _tapState = TAP_SPIKE;
                _spikeStartTime = now;
            }
            break;
            
        case TAP_SPIKE:
            if (deviation < TAP_QUIET_THRESHOLD_G) {
                unsigned long duration = now - _spikeStartTime;
                if (duration >= TAP_MIN_DURATION_MS && duration <= TAP_MAX_DURATION_MS) {
                    _tapState = TAP_QUIET;
                    _quietStartTime = now;
                } else {
                    _tapState = TAP_IDLE; // Took too long, not a tap
                }
            } else if (now - _spikeStartTime > TAP_MAX_DURATION_MS) {
                _tapState = TAP_IDLE; // Sustained movement
            }
            break;
            
        case TAP_QUIET:
            if (deviation > TAP_QUIET_THRESHOLD_G) {
                _tapState = TAP_IDLE; // Not quiet, failed
            } else if (now - _quietStartTime >= TAP_QUIET_PERIOD_MS) {
                _tapState = TAP_WAIT_DOUBLE;
                _firstTapTime = now;
            }
            break;
            
        case TAP_WAIT_DOUBLE:
            if (deviation > _tapThresholdG) {
                // Second tap detected!
                DBG_GEST("Gesture: DOUBLE_TAP\n");
                _lastGesture = GESTURE_DOUBLE_TAP;
                _lastGestureTime = now;
                _tapState = TAP_IDLE;
            } else if (now - _firstTapTime > DOUBLE_TAP_WINDOW_MS) {
                // Timeout, register as single tap
                DBG_GEST("Gesture: SINGLE_TAP\n");
                _lastGesture = GESTURE_SINGLE_TAP;
                _lastGestureTime = now;
                _tapState = TAP_IDLE;
            }
            break;
    }
}

GestureType GestureManager::getGesture() {
    GestureType g = _lastGesture;
    _lastGesture = GESTURE_NONE;
    return g;
}

bool GestureManager::hasGesture() const {
    return _lastGesture != GESTURE_NONE;
}
