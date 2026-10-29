/*
 * ============================================================================
 * Types.h — Shared Type Definitions for ESP32-C3 Fitness Tracker
 * ============================================================================
 *
 * All enums, structs, and type definitions used across modules.
 *
 * Auto-Detection Architecture:
 *   Each exercise has a set of PositionProfiles (learned sensor ranges).
 *   The AutoDetector compares live data against all ExerciseProfiles
 *   and counts reps when a complete position sequence is detected.
 *
 * ============================================================================
 */

#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ============================================================================
// LIMITS
// ============================================================================
#define MAX_POSITIONS_PER_EXERCISE  4   // Max unique positions in one exercise
#define MAX_EXERCISES               4   // Walking, Squat, JJ, SitUp

// ============================================================================
// EXERCISE TYPES
// ============================================================================
enum ExerciseType {
    EXERCISE_UNKNOWN = 0,
    EXERCISE_WALKING,        // 1
    EXERCISE_SQUAT,          // 2
    EXERCISE_JUMPING_JACK,   // 3
    EXERCISE_SIT_UP          // 4
};

// Helper: convert ExerciseType to 0-based array index
// EXERCISE_WALKING(1) → 0, EXERCISE_SQUAT(2) → 1, etc.
inline int exerciseIndex(ExerciseType t) { return (int)t - 1; }

// Helper: convert 0-based index back to ExerciseType
inline ExerciseType exerciseFromIndex(int idx) { return (ExerciseType)(idx + 1); }

// ============================================================================
// APPLICATION STATES
// ============================================================================
enum AppState {
    APP_SPLASH = 0,
    APP_AUTO_DETECT,     // Main mode: automatic exercise detection + counting
    APP_MENU,            // Settings menu
    APP_CAL_MENU,        // Calibration sub-menu
    APP_CALIBRATING,     // Calibration in progress
    APP_RESULTS          // View exercise counts
};

// ============================================================================
// MENU ITEMS (simplified — no manual exercise selection)
// ============================================================================
enum MenuItem {
    MENU_RESUME = 0,     // Return to auto-detect
    MENU_CALIBRATE,      // Enter calibration sub-menu
    MENU_RESULTS,        // View accumulated counts
    MENU_RESET,          // Reset all data
    MENU_ITEM_COUNT      // sentinel
};

// ============================================================================
// CALIBRATION SUB-MENU ITEMS
// ============================================================================
enum CalMenuItem {
    CAL_MENU_WALKING = 0,
    CAL_MENU_SQUAT,
    CAL_MENU_JJ,
    CAL_MENU_SITUP,
    CAL_MENU_ALL,
    CAL_MENU_BACK,
    CAL_MENU_ITEM_COUNT  // sentinel
};

// ============================================================================
// GESTURE TYPES
// ============================================================================
enum GestureType {
    GESTURE_NONE = 0,
    GESTURE_SINGLE_TAP,
    GESTURE_DOUBLE_TAP,
    GESTURE_SHAKE
};

// ============================================================================
// SENSOR MOUNTING
// ============================================================================
enum SensorSide {
    LEFT_HIP = 0,
    RIGHT_HIP = 1
};

struct SensorAxisConfig {
    SensorSide side;
    int8_t  xSign, ySign, zSign;
    uint8_t xAxis, yAxis, zAxis;
};

// ============================================================================
// SENSOR DATA STRUCTURES
// ============================================================================

struct RawSensorData {
    float ax, ay, az;         // Accelerometer (m/s²)
    float gx, gy, gz;         // Gyroscope (deg/s)
    unsigned long timestamp;
};

struct MappedSensorData {
    float ax, ay, az;         // Accel in world frame (m/s²)
    float gx, gy, gz;         // Gyro in world frame (deg/s)
    unsigned long timestamp;
};

struct FilteredData {
    float ax, ay, az;         // Smoothed accel (world frame)
    float gx, gy, gz;         // Smoothed gyro (world frame)
    float pitch;              // Forward/backward tilt (degrees)
    float roll;               // Side-to-side tilt (degrees)
    float linAx, linAy, linAz; // Linear acceleration (gravity removed)
    float gravX, gravY, gravZ;  // Estimated gravity vector
    unsigned long timestamp;
};

struct SensorFeatures {
    float accelMagnitude;        // Instantaneous accel magnitude (m/s²)
    float gyroMagnitude;         // Instantaneous gyro magnitude (deg/s)
    float accelMagMean;          // Mean accel magnitude over window
    float accelMagVariance;      // Variance of accel magnitude
    float accelMagPeak;          // Max accel magnitude in window
    float gyroMagMean;           // Mean gyro magnitude over window
    float gyroMagPeak;           // Max gyro magnitude in window
    float pitch;                 // Current pitch angle (degrees)
    float roll;                  // Current roll angle (degrees)
    float pitchRate;             // Rate of pitch change (deg/s)
    float rollRate;              // Rate of roll change (deg/s)
    float verticalAccel;         // Vertical linear acceleration
    float movementEnergy;        // Variance-based movement energy
    bool  isPeak;
    bool  isValley;
    float lastPeakValue;
    float lastValleyValue;
    float peakToValley;
    unsigned long lastPeakTime;
    unsigned long lastValleyTime;
    float dominantPeriodMs;
    int   peakCount;
    int   zeroCrossingCount;
    unsigned long timestamp;
};

// ============================================================================
// POSITION PROFILE — A learned body position with statistical ranges
// ============================================================================
/*
 * During calibration, the system collects multiple sensor samples while the
 * user holds a position (or during movement peaks). It computes statistics
 * for each feature, creating a tolerance band.
 *
 * During exercise, a live reading "matches" a position if the sensor values
 * fall within the learned range (mean ± tolerance based on stddev).
 */
struct PositionProfile {
    char  name[16];           // e.g. "Standing", "Squat Down"

    float pitchMean;          // degrees
    float pitchStdDev;
    float pitchMin;
    float pitchMax;

    float rollMean;           // degrees
    float rollStdDev;
    float rollMin;
    float rollMax;

    float accelMagMean;       // m/s²
    float accelMagStdDev;
    float accelMagMin;
    float accelMagMax;

    float gyroMagMean;        // deg/s
    float gyroMagStdDev;
    float gyroMagMin;
    float gyroMagMax;

    uint16_t sampleCount;     // How many samples were used
    bool isValid;             // True if successfully calibrated
};

// ============================================================================
// EXERCISE PROFILE — Complete exercise definition with position sequence
// ============================================================================
/*
 * Each exercise has positionCount unique positions. One repetition is
 * defined as the cycle: positions[0] → positions[1] → positions[0].
 *
 * For all current exercises (walking, squat, JJ, sit-up), positionCount=2.
 * The cycle is always P0 → P1 → P0 = 1 rep.
 *
 * Future exercises with more positions (e.g., burpees) could have
 * positionCount=3 with cycle P0 → P1 → P2 → P0.
 */
struct ExerciseProfile {
    ExerciseType type;
    char name[16];            // e.g. "Squat"

    uint8_t positionCount;    // Number of unique positions (2 for all current exercises)
    PositionProfile positions[MAX_POSITIONS_PER_EXERCISE];

    // Timing constraints (learned from calibration reps)
    float minRepDurationMs;   // Fastest valid rep
    float maxRepDurationMs;   // Slowest valid rep (timeout)
    float avgRepDurationMs;   // Typical rep duration

    bool isCalibrated;        // True if fully calibrated
};

// ============================================================================
// POSITION MATCH RESULT
// ============================================================================
struct PositionMatchResult {
    float similarity;         // 0.0 to 1.0 (how close to the learned position)
    bool  isWithinRange;      // True if within padded min/max bounds
};

// ============================================================================
// AUTO-DETECTOR STATES
// ============================================================================
enum AutoDetectState {
    AUTO_IDLE = 0,            // No exercise detected — scanning all profiles
    AUTO_CANDIDATE,           // Tentative exercise identified, confirming
    AUTO_TRACKING,            // Confirmed — tracking position sequence for reps
    AUTO_COOLDOWN             // Brief pause after counting a rep
};

// ============================================================================
// USER PROFILE DATA (stored in NVS)
// ============================================================================
struct UserProfileData {
    ExerciseProfile exercises[MAX_EXERCISES];   // [0]=Walking [1]=Squat [2]=JJ [3]=SitUp
    uint32_t exerciseCounts[MAX_EXERCISES];     // Persistent rep counts (same indexing)
    uint32_t magic;                             // Data integrity check
};

// ============================================================================
// CALIBRATION FLOW STATES
// ============================================================================
enum CalibrationPhase {
    CAL_PHASE_IDLE = 0,
    CAL_PHASE_WAITING,         // "Get into position, tap when ready"
    CAL_PHASE_COUNTDOWN,       // 3-2-1 countdown
    CAL_PHASE_RECORDING_POS,   // Recording samples for a static position
    CAL_PHASE_RECORDING_REPS,  // Recording full reps (timing + dynamic P1 extraction)
    CAL_PHASE_PROCESSING,      // Computing statistics
    CAL_PHASE_COMPLETE,        // Done — showing results
    CAL_PHASE_FAILED           // Calibration failed (timeout, not enough data)
};

// ============================================================================
// HELPER FUNCTION DECLARATIONS
// ============================================================================
const char* exerciseTypeName(ExerciseType t);
const char* autoDetectStateName(AutoDetectState s);
const char* calPhaseName(CalibrationPhase p);

#endif // TYPES_H
