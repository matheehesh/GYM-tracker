/*
 * ============================================================================
 * ButtonManager.h — Physical Push Button Input for ESP32-C3 Fitness Tracker
 * ============================================================================
 *
 * Detects press patterns on a physical GPIO button and maps each pattern
 * to an EXISTING GestureType action used by the rest of the application.
 *
 * This is ONLY an input method. It does not create new actions.
 * The mapping from button interaction → GestureType is configured in Config.h.
 *
 * Architecture:
 *   GPIO pin (INPUT_PULLUP, active LOW)
 *       ↓
 *   Debounce + pattern detection (internal, isolated)
 *       ↓
 *   Map to GestureType via BUTTON_*_ACTION defines in Config.h
 *       ↓
 *   getGesture() returns the same GestureType the app already understands
 *
 * ============================================================================
 */

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"

class ButtonManager {
public:
    void begin();
    void update();                  // Call every loop iteration (non-blocking)
    GestureType getGesture();       // Returns and CLEARS last detected gesture
    bool hasGesture() const;        // Check without clearing

private:
    // Internal pattern detection states — NOT exposed to the rest of the app
    enum ButtonPhase {
        BTN_IDLE,           // Waiting for press
        BTN_PRESSED,        // Button down, timing hold duration
        BTN_RELEASED_ONCE,  // Released after first press, waiting for possible second press
        BTN_PRESSED_SECOND  // Second press detected, waiting for release
    };

    ButtonPhase _phase;
    GestureType _pendingGesture;    // The mapped GestureType to deliver

    bool        _lastRawState;      // Last debounced state (true = pressed)
    bool        _stableState;       // Current debounced state
    unsigned long _lastChangeTime;  // For debounce timing
    unsigned long _pressStartTime;  // When the current/first press began
    unsigned long _releaseTime;     // When first release happened
};

#endif // BUTTON_MANAGER_H
