/*
 * ============================================================================
 * fitness_tracker.ino — Main Sketch for ESP32-C3 Auto-Detecting Fitness Tracker
 * ============================================================================
 *
 * Architecture:
 *   Power On → Load calibration → Auto-detect exercises → Count reps
 *
 *   Sensor → Filter → Features → AutoDetector (compares ALL profiles)
 *                                → Identifies exercise → Tracks sequence → Counts
 *
 * Input:  Physical button (GPIO 4) + MPU6050 tap/shake gestures
 * Output: SSD1306 OLED display + Serial debug
 *
 * ============================================================================
 */

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "SensorManager.h"
#include "SignalFilter.h"
#include "FeatureExtractor.h"
#include "UserProfile.h"
#include "CalibrationManager.h"
#include "AutoDetector.h"
#include "DisplayManager.h"
#include "GestureManager.h"
#include "ButtonManager.h"

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
SensorManager sensor;
SignalFilter filter;
FeatureExtractor features;
UserProfile profile;
CalibrationManager calibration;
AutoDetector autoDetector;
DisplayManager display;
GestureManager gesture;
ButtonManager button;

// ============================================================================
// GLOBAL STATE
// ============================================================================
AppState appState = APP_SPLASH;
MenuItem menuSelection = MENU_RESUME;
CalMenuItem calMenuSelection = CAL_MENU_WALKING;
unsigned long lastSensorRead = 0;
unsigned long splashStartTime = 0;
unsigned long lastDebugPrint = 0;  // For periodic [RAW SENSOR] debug output

// Latest sensor data (kept for display updates between sensor reads)
SensorFeatures latestFeatures;
FilteredData latestFiltered;

// Forward declarations
void processAppState(const SensorFeatures& feat, const FilteredData& filt);
void updateDisplay();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    delay(100);

    DBG_PRINTLN("==================================");
    DBG_PRINTLN(" GYM TRACKER v2.0 — AUTO DETECT  ");
    DBG_PRINTLN("==================================");

    // Initialize display
    if (!display.begin()) {
        DBG_PRINTLN("OLED init failed!");
        while (1) delay(10);
    }
    display.showSplash();

    // Initialize MPU6050
    if (!sensor.begin()) {
        DBG_PRINTLN("MPU6050 init failed!");
        display.showMessage("SENSOR ERROR", "Check MPU6050", "wiring!");
        while (1) delay(10);
    }

    // Calibrate sensor offsets (user must hold still)
    display.showMessage("CALIBRATING", "Hold still...");
    sensor.calibrateOffsets();

    // Initialize processing pipeline
    filter.begin();
    features.begin();

    // Load user profile from NVS (calibration data + rep counts)
    profile.begin();
    profile.printCalibration();

    // Initialize auto-detector with loaded profile
    autoDetector.begin(&profile.data);

    // Restore persisted rep counts into auto-detector
    // (AutoDetector tracks counts in RAM; we sync from NVS on boot)
    // Counts are stored in profile.data.exerciseCounts and displayed from there

    // Initialize calibration manager
    calibration.begin(&profile);

    // Initialize input methods
    gesture.begin();
    button.begin();

    // Start splash timer
    splashStartTime = millis();
    appState = APP_SPLASH;

    DBG_PRINTLN("Setup complete.");
    if (profile.isAnyCalibrated()) {
        DBG_PRINTLN("Calibration found — will enter AUTO DETECT mode.");
    } else {
        DBG_PRINTLN("No calibration found — will enter CALIBRATION menu.");
    }
}

// ============================================================================
// MAIN LOOP (non-blocking)
// ============================================================================
void loop() {
    unsigned long now = millis();

    // 1. Update button every loop iteration (fast GPIO read)
    button.update();

    // 2. Read sensor at SENSOR_SAMPLE_RATE_HZ
    if (now - lastSensorRead >= SENSOR_SAMPLE_INTERVAL_MS) {
        lastSensorRead = now;
        float dt = SENSOR_SAMPLE_INTERVAL_MS / 1000.0f;

        RawSensorData raw;
        if (sensor.read(raw)) {
            MappedSensorData mapped = sensor.mapAxes(raw);
            latestFiltered = filter.update(mapped, dt);
            latestFeatures = features.update(latestFiltered, dt);

            // Periodic [RAW SENSOR] debug output
            if (now - lastDebugPrint >= DEBUG_PIPELINE_MS) {
                lastDebugPrint = now;
                DBG_PRINTF("[RAW SENSOR] ax=%.2f ay=%.2f az=%.2f gx=%.1f gy=%.1f gz=%.1f\n",
                           raw.ax, raw.ay, raw.az, raw.gx, raw.gy, raw.gz);
                DBG_PRINTF("[FILTERED]   pitch=%.1f roll=%.1f accelMag=%.2f gyroMag=%.1f\n",
                           latestFiltered.pitch, latestFiltered.roll,
                           latestFeatures.accelMagnitude, latestFeatures.gyroMagnitude);
            }

            // Update gesture detector
            gesture.update(latestFeatures.accelMagnitude);

            // Process based on app state
            processAppState(latestFeatures, latestFiltered);
        }
    }

    // 3. Update display at DISPLAY_REFRESH_MS
    if (display.needsRefresh(now)) {
        updateDisplay();
        display.markRefreshed();
    }
}

// ============================================================================
// APP STATE MACHINE
// ============================================================================
void processAppState(const SensorFeatures& feat, const FilteredData& filt) {
    // Merge inputs: physical button + MPU6050 gesture → single GestureType
    GestureType g = gesture.getGesture();
    GestureType bg = button.getGesture();
    if (bg != GESTURE_NONE) g = bg;

    switch (appState) {

        // ------------------------------------------------------------------
        // SPLASH SCREEN — wait then auto-route
        // ------------------------------------------------------------------
        case APP_SPLASH:
            if (millis() - splashStartTime >= SPLASH_DURATION_MS || g == GESTURE_SINGLE_TAP) {
                if (profile.isAnyCalibrated()) {
                    appState = APP_AUTO_DETECT;
                    gesture.setEnabled(false);  // Disable tap detection during tracking
                    DBG_PRINTLN("[APP] -> AUTO_DETECT");
                } else {
                    appState = APP_CAL_MENU;
                    DBG_PRINTLN("[APP] -> CAL_MENU (no calibration found)");
                }
            }
            break;

        // ------------------------------------------------------------------
        // AUTO-DETECT MODE — main tracking screen
        // ------------------------------------------------------------------
        case APP_AUTO_DETECT:
            // Feed sensor data to auto-detector
            autoDetector.update(feat);

            // Any button press → go to menu
            if (g == GESTURE_SINGLE_TAP || g == GESTURE_DOUBLE_TAP || g == GESTURE_SHAKE) {
                gesture.setEnabled(true);
                gesture.setHighSensitivity(true);
                
                // Transfer accumulated rep counts to persistent profile before leaving
                for (int i = 0; i < MAX_EXERCISES; i++) {
                    profile.data.exerciseCounts[i] += autoDetector.getRepCount(exerciseFromIndex(i));
                }
                autoDetector.resetCounts();
                profile.saveToNVS();
                
                appState = APP_MENU;
                menuSelection = MENU_RESUME;
                DBG_PRINTLN("[APP] AUTO_DETECT -> MENU");
            }
            break;

        // ------------------------------------------------------------------
        // SETTINGS MENU
        // ------------------------------------------------------------------
        case APP_MENU:
            if (g == GESTURE_SINGLE_TAP) {
                menuSelection = (MenuItem)((menuSelection + 1) % MENU_ITEM_COUNT);
            } else if (g == GESTURE_DOUBLE_TAP) {
                switch (menuSelection) {
                    case MENU_RESUME:
                        if (profile.isAnyCalibrated()) {
                            appState = APP_AUTO_DETECT;
                            gesture.setEnabled(false);
                            DBG_PRINTLN("[APP] MENU -> AUTO_DETECT");
                        } else {
                            display.showMessage("Calibrate", "first!");
                            delay(1000);
                        }
                        break;

                    case MENU_CALIBRATE:
                        appState = APP_CAL_MENU;
                        calMenuSelection = CAL_MENU_WALKING;
                        DBG_PRINTLN("[APP] MENU -> CAL_MENU");
                        break;

                    case MENU_RESULTS:
                        appState = APP_RESULTS;
                        DBG_PRINTLN("[APP] MENU -> RESULTS");
                        break;

                    case MENU_RESET:
                        profile.resetAll();
                        autoDetector.resetCounts();
                        autoDetector.reset();
                        display.showMessage("All data", "reset!");
                        delay(1000);
                        appState = APP_MENU;
                        DBG_PRINTLN("[APP] All data reset");
                        break;

                    default:
                        break;
                }
            } else if (g == GESTURE_SHAKE) {
                // Shake = go back to auto-detect (if calibrated)
                if (profile.isAnyCalibrated()) {
                    appState = APP_AUTO_DETECT;
                    gesture.setEnabled(false);
                }
            }
            break;

        // ------------------------------------------------------------------
        // CALIBRATION SUB-MENU
        // ------------------------------------------------------------------
        case APP_CAL_MENU:
            if (g == GESTURE_SINGLE_TAP) {
                calMenuSelection = (CalMenuItem)((calMenuSelection + 1) % CAL_MENU_ITEM_COUNT);
            } else if (g == GESTURE_DOUBLE_TAP) {
                if (calMenuSelection == CAL_MENU_BACK) {
                    appState = APP_MENU;
                    DBG_PRINTLN("[APP] CAL_MENU -> MENU");
                } else {
                    ExerciseType calType = EXERCISE_UNKNOWN;
                    if (calMenuSelection == CAL_MENU_WALKING) calType = EXERCISE_WALKING;
                    else if (calMenuSelection == CAL_MENU_SQUAT) calType = EXERCISE_SQUAT;
                    else if (calMenuSelection == CAL_MENU_JJ) calType = EXERCISE_JUMPING_JACK;
                    else if (calMenuSelection == CAL_MENU_SITUP) calType = EXERCISE_SIT_UP;
                    else if (calMenuSelection == CAL_MENU_ALL) calType = EXERCISE_WALKING; // Start with first

                    if (calType != EXERCISE_UNKNOWN) {
                        calibration.startCalibration(calType);
                        appState = APP_CALIBRATING;
                        DBG_PRINTF("[APP] CAL_MENU -> CALIBRATING (%s)\n", exerciseTypeName(calType));
                    }
                }
            } else if (g == GESTURE_SHAKE) {
                appState = APP_MENU;
                DBG_PRINTLN("[APP] CAL_MENU -> MENU (shake)");
            }
            break;

        // ------------------------------------------------------------------
        // CALIBRATING — position-based calibration in progress
        // ------------------------------------------------------------------
        case APP_CALIBRATING:
            calibration.update(feat, filt, g);

            if (!calibration.isActive()) {
                // Calibration done — reinitialize auto-detector with new data
                autoDetector.setProfile(&profile.data);
                autoDetector.reset();

                DBG_PRINTLN("[APP] Calibration complete — profile updated");

                // If calibrating ALL, advance to next exercise
                if (calMenuSelection == CAL_MENU_ALL &&
                    calibration.getActiveExercise() != EXERCISE_SIT_UP) {
                    ExerciseType nextType = (ExerciseType)(calibration.getActiveExercise() + 1);
                    calibration.startCalibration(nextType);
                    DBG_PRINTF("[APP] Advancing to next: %s\n", exerciseTypeName(nextType));
                } else {
                    // Done — go to auto-detect if calibrated, else stay in cal menu
                    if (profile.isAnyCalibrated()) {
                        appState = APP_AUTO_DETECT;
                        gesture.setEnabled(false);
                        DBG_PRINTLN("[APP] CALIBRATING -> AUTO_DETECT");
                    } else {
                        appState = APP_CAL_MENU;
                        DBG_PRINTLN("[APP] CALIBRATING -> CAL_MENU");
                    }
                }
            }
            break;

        // ------------------------------------------------------------------
        // RESULTS — view accumulated counts
        // ------------------------------------------------------------------
        case APP_RESULTS:
            if (g == GESTURE_SINGLE_TAP || g == GESTURE_DOUBLE_TAP || g == GESTURE_SHAKE) {
                appState = APP_MENU;
                DBG_PRINTLN("[APP] RESULTS -> MENU");
            }
            break;
    }
}

// ============================================================================
// DISPLAY UPDATE
// ============================================================================
void updateDisplay() {
#if DEBUG_MODE && DEBUG_AXIS_MAPPING
    display.showAxisDebug(latestFiltered.linAx, latestFiltered.linAy, latestFiltered.linAz,
                          latestFiltered.pitch, latestFiltered.roll);
    return;
#endif

    switch (appState) {
        case APP_SPLASH:
            // Splash is shown once in setup; don't re-draw
            break;

        case APP_AUTO_DETECT: {
            ExerciseType det = autoDetector.getDetectedExercise();
            const char* name = (det != EXERCISE_UNKNOWN) ? exerciseTypeName(det) : "";
            // Show cumulative counts: persisted + current detector count
            uint32_t reps = 0;
            if (det != EXERCISE_UNKNOWN) {
                int idx = exerciseIndex(det);
                reps = profile.data.exerciseCounts[idx] + autoDetector.getRepCount(det);
            }
            display.showAutoDetect(name, reps, autoDetector.getConfidence(), autoDetector.getState());
            break;
        }

        case APP_MENU:
            display.showMenu(menuSelection);
            break;

        case APP_CAL_MENU:
            display.showCalMenu(calMenuSelection);
            break;

        case APP_CALIBRATING:
            display.showCalibration(
                calibration.getActiveExercise(),
                calibration.getPhase(),
                calibration.getInstruction(),
                calibration.getProgress(),
                calibration.getDetectedReps()
            );
            break;

        case APP_RESULTS:
            display.showResults(profile.data.exerciseCounts);
            break;
    }
}
