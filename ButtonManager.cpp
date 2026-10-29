

#include "ButtonManager.h"

void ButtonManager::begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    _phase = BTN_IDLE;
    _pendingGesture = GESTURE_NONE;
    _lastRawState = false;
    _stableState = false;
    _lastChangeTime = 0;
    _pressStartTime = 0;
    _releaseTime = 0;
}

void ButtonManager::update() {
    unsigned long now = millis();

    // --- Debounce ---
    // Button is active LOW (INPUT_PULLUP: pressed = LOW = digitalRead 0)
    bool rawPressed = (digitalRead(BUTTON_PIN) == LOW);

    if (rawPressed != _lastRawState) {
        _lastChangeTime = now;
    }
    _lastRawState = rawPressed;

    // Only accept state change after debounce period
    bool pressed;
    if ((now - _lastChangeTime) >= BUTTON_DEBOUNCE_MS) {
        pressed = rawPressed;
    } else {
        pressed = _stableState; // Keep previous state during debounce
    }

    bool justPressed  = (pressed && !_stableState);
    bool justReleased = (!pressed && _stableState);
    _stableState = pressed;

    // --- Pattern detection state machine ---
    switch (_phase) {

        case BTN_IDLE:
            if (justPressed) {
                _pressStartTime = now;
                _phase = BTN_PRESSED;
            }
            break;

        case BTN_PRESSED:
            // Check for hold while button is still down
            if (pressed && (now - _pressStartTime >= BUTTON_HOLD_MS)) {
                // Long hold detected — map and emit immediately
                _pendingGesture = (GestureType)BUTTON_HOLD_ACTION;
                _phase = BTN_IDLE;
                // Wait for release before allowing new input
                // (handled by debounce + IDLE requiring justPressed)
                DBG_GEST("[BTN] HOLD -> action %d\n", _pendingGesture);
            }
            else if (justReleased) {
                // Short press ended — might be single or start of double
                _releaseTime = now;
                _phase = BTN_RELEASED_ONCE;
            }
            break;

        case BTN_RELEASED_ONCE:
            if (justPressed) {
                // Second press within window — it's a double press
                _phase = BTN_PRESSED_SECOND;
            }
            else if (now - _releaseTime >= BUTTON_DOUBLE_WINDOW_MS) {
                // Timeout — no second press arrived — emit single press action
                _pendingGesture = (GestureType)BUTTON_PRESS_ACTION;
                _phase = BTN_IDLE;
                DBG_GEST("[BTN] PRESS -> action %d\n", _pendingGesture);
            }
            break;

        case BTN_PRESSED_SECOND:
            if (justReleased) {
                // Second press released — emit double press action
                _pendingGesture = (GestureType)BUTTON_DOUBLE_ACTION;
                _phase = BTN_IDLE;
                DBG_GEST("[BTN] DOUBLE -> action %d\n", _pendingGesture);
            }
            else if (now - _pressStartTime >= BUTTON_HOLD_MS) {
                // Held during second press — treat as hold instead
                _pendingGesture = (GestureType)BUTTON_HOLD_ACTION;
                _phase = BTN_IDLE;
                DBG_GEST("[BTN] HOLD (2nd) -> action %d\n", _pendingGesture);
            }
            break;
    }
}

GestureType ButtonManager::getGesture() {
    GestureType g = _pendingGesture;
    _pendingGesture = GESTURE_NONE;
    return g;
}

bool ButtonManager::hasGesture() const {
    return _pendingGesture != GESTURE_NONE;
}
