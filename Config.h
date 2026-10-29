/*
 * ============================================================================
 * Config.h — Master Configuration for ESP32-C3 Fitness Tracker
 * ============================================================================
 *
 * ALL tunable values are defined here. After running calibration, you can
 * override the auto-calculated thresholds by modifying the DEFAULT values
 * below and re-uploading. The system uses calibrated (NVS) values when
 * available, falling back to these defaults otherwise.
 *
 * Hardware: ESP32-C3 SuperMini + MPU6050 (hip-mounted) + SSD1306 OLED
 * Input: Physical push button (GPIO 4) + MPU6050 tap/shake gestures
 *
 * ============================================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// DEBUG CONFIGURATION
// ============================================================================
// Set to 1 to enable Serial debug output. Set to 0 for production.
// Sub-categories only take effect when DEBUG_MODE is 1.

#define DEBUG_MODE            1   // Master debug switch
#define DEBUG_RAW_SENSOR      0   // Print raw ax,ay,az,gx,gy,gz every cycle
#define DEBUG_FILTERED        0   // Print filtered accel, gyro, angles
#define DEBUG_FEATURES        0   // Print extracted features
#define DEBUG_STATE           1   // Print state transitions for all detectors
#define DEBUG_COUNT           1   // Print repetition/step counts
#define DEBUG_CALIBRATION     1   // Print calibration values and progress
#define DEBUG_GESTURES        0   // Print gesture detection events
#define DEBUG_AXIS_MAPPING    0   // Print mapped axis values (for verifying mount)
#define DEBUG_AUTO_DETECT     1   // Print auto-detection state, matches, confidence

// ============================================================================
// DEBUG MACROS
// ============================================================================
#if DEBUG_MODE
  #define DBG_PRINT(x)          Serial.print(x)
  #define DBG_PRINTLN(x)        Serial.println(x)
  #define DBG_PRINTF(fmt, ...)  Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_RAW_SENSOR
  #define DBG_RAW(fmt, ...)     Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_RAW(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_FILTERED
  #define DBG_FILT(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_FILT(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_FEATURES
  #define DBG_FEAT(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_FEAT(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_STATE
  #define DBG_STATE(fmt, ...)   Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_STATE(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_COUNT
  #define DBG_COUNT(fmt, ...)   Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_COUNT(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_CALIBRATION
  #define DBG_CAL(fmt, ...)     Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_CAL(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_GESTURES
  #define DBG_GEST(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_GEST(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_AXIS_MAPPING
  #define DBG_AXIS(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_AXIS(fmt, ...)
#endif

#if DEBUG_MODE && DEBUG_AUTO_DETECT
  #define DBG_AUTO(fmt, ...)    Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DBG_AUTO(fmt, ...)
#endif

// ============================================================================
// I2C PIN CONFIGURATION
// ============================================================================
// ESP32-C3 SuperMini: GPIO 8 (SDA) and GPIO 9 (SCL)
// NOTE: GPIO 8 and 9 are strapping pins. Most SuperMini boards have pull-ups.
// If you experience boot issues, add external 4.7kΩ pull-ups to 3.3V,
// or rewire to GPIO 4 (SDA) / GPIO 5 (SCL) and change these values.

#define I2C_SDA_PIN           8
#define I2C_SCL_PIN           9
#define I2C_CLOCK_HZ          400000   // 400 kHz Fast Mode

// ============================================================================
// DISPLAY CONFIGURATION (SSD1306 128x64 OLED, 4-pin I2C)
// ============================================================================
#define SCREEN_WIDTH          128
#define SCREEN_HEIGHT         64
#define OLED_I2C_ADDRESS      0x3C     // Common SSD1306 address (try 0x3D if this fails)
#define OLED_RESET_PIN        -1       // No reset pin (4-pin module)
#define DISPLAY_REFRESH_MS    100      // Display update interval (10 Hz)

// ============================================================================
// MPU6050 CONFIGURATION
// ============================================================================
#define MPU_I2C_ADDRESS       0x68     // Default MPU6050 address (0x69 if AD0=HIGH)
#define SENSOR_SAMPLE_RATE_HZ 100      // Target sensor read rate
#define SENSOR_SAMPLE_INTERVAL_MS  (1000 / SENSOR_SAMPLE_RATE_HZ)

// MPU6050 range settings (Adafruit library enums used in SensorManager)
// Accelerometer: MPU6050_RANGE_2_G, _4_G, _8_G, _16_G
// Gyroscope: MPU6050_RANGE_250_DEG, _500_DEG, _1000_DEG, _2000_DEG
// Higher range = less precision but won't clip during fast movement
#define MPU_ACCEL_RANGE       2   // 0=2G, 1=4G, 2=8G, 3=16G
#define MPU_GYRO_RANGE        1   // 0=250, 1=500, 2=1000, 3=2000 deg/s
#define MPU_FILTER_BW         3   // 0=260Hz,1=184Hz,2=94Hz,3=44Hz,4=21Hz,5=10Hz

// Sensor offset calibration (hold still at startup)
#define SENSOR_OFFSET_SAMPLES 200  // Number of samples to average for offset
#define SENSOR_OFFSET_SETTLE_MS 1000 // Wait before sampling offsets

// ============================================================================
// SENSOR MOUNTING ORIENTATION — CONFIGURABLE
// ============================================================================
/*
 * IMPORTANT: This section configures how the MPU6050 is physically mounted.
 *
 * The exercise algorithms use a WORLD coordinate frame:
 *   World X = FORWARD  (direction the person faces)
 *   World Y = UP       (opposite to gravity)
 *   World Z = RIGHT    (to the person's right side)
 *
 * The MPU6050 has its own chip coordinate frame (marked on the breakout board):
 *   Chip X, Chip Y, Chip Z
 *
 * You need to configure which CHIP axis maps to each WORLD axis, and whether
 * the sign needs to be inverted.
 *
 * HOW TO CONFIGURE:
 * 1. Mount the MPU6050 on your hip
 * 2. Enable DEBUG_AXIS_MAPPING = 1 (above)
 * 3. Upload and open Serial Monitor
 * 4. Stand upright and still — note which raw axis reads ~9.81 m/s² (that's gravity)
 * 5. The axis reading ~9.81 downward maps to World Y (but inverted, since gravity is -Y)
 * 6. Lean forward — the axis that increases maps to World X
 * 7. Lean right — the axis that increases maps to World Z
 *
 * AXIS_MAP_X/Y/Z: Which raw axis (0=rawX, 1=rawY, 2=rawZ) maps to world X/Y/Z
 * AXIS_SIGN_X/Y/Z: +1 or -1 to flip direction
 *
 * EXAMPLE MOUNTING CONFIGURATIONS:
 *
 * Config A: MPU6050 chip face outward, top of chip pointing UP, on RIGHT hip
 *   AXIS_MAP = {0, 1, 2}, SIGNS = {+1, +1, +1}
 *
 * Config B: MPU6050 chip face outward, top pointing FORWARD, on RIGHT hip
 *   AXIS_MAP = {1, 0, 2}, SIGNS = {+1, +1, +1}  (X↔Y swap)
 *
 * Config C: MPU6050 chip face toward body, top pointing UP, on RIGHT hip
 *   AXIS_MAP = {0, 1, 2}, SIGNS = {-1, +1, -1}  (X,Z inverted)
 *
 * Config D: On LEFT hip (mirror Z axis):
 *   Same as right hip but flip AXIS_SIGN_Z
 */

// Which hip side the sensor is mounted on
#define SENSOR_SIDE           1   // 0 = LEFT_HIP, 1 = RIGHT_HIP

// Axis mapping: which raw MPU axis maps to each world axis
// 0 = raw X,  1 = raw Y,  2 = raw Z
#define AXIS_MAP_WORLD_X      0   // Which raw axis is FORWARD
#define AXIS_MAP_WORLD_Y      1   // Which raw axis is UP
#define AXIS_MAP_WORLD_Z      2   // Which raw axis is RIGHT

// Axis signs: +1 = same direction, -1 = inverted
#define AXIS_SIGN_WORLD_X     1   // +1 or -1
#define AXIS_SIGN_WORLD_Y     1   // +1 or -1
#define AXIS_SIGN_WORLD_Z     1   // +1 or -1

// ============================================================================
// SIGNAL FILTER CONFIGURATION
// ============================================================================

// Complementary filter: fuses accelerometer (long-term stable) with
// gyroscope (short-term smooth). Alpha close to 1.0 trusts gyro more.
// angle = alpha * (angle + gyro * dt) + (1 - alpha) * accel_angle
#define COMP_FILTER_ALPHA     0.96f

// Low-pass filter for acceleration smoothing
// Higher alpha = smoother but more lag. Range: 0.0 to 1.0
#define LPF_ACCEL_ALPHA       0.3f

// Low-pass filter for gyroscope smoothing
#define LPF_GYRO_ALPHA        0.3f

// High-pass filter alpha for removing gravity from acceleration
#define HPF_ALPHA             0.8f

// ============================================================================
// GESTURE DETECTION (replaces physical buttons)
// ============================================================================
/*
 * Since there are no physical buttons, navigation uses MPU6050 gestures:
 *   SINGLE TAP  = Navigate to next menu item / acknowledge prompt
 *   DOUBLE TAP  = Select / confirm
 *   SHAKE       = Back / stop exercise / return to menu
 *
 * Tap detection: sudden acceleration spike above TAP_THRESHOLD
 * Shake detection: sustained high acceleration for SHAKE_DURATION_MS
 */

#define TAP_THRESHOLD_G       1.8f    // Acceleration spike threshold (in g above baseline)
#define TAP_QUIET_THRESHOLD_G 0.5f    // Must drop below this after tap spike
#define TAP_MIN_DURATION_MS   10      // Minimum spike duration to be a tap
#define TAP_MAX_DURATION_MS   150     // Maximum spike duration (longer = not a tap)
#define TAP_QUIET_PERIOD_MS   100     // Must be quiet this long after spike
#define DOUBLE_TAP_WINDOW_MS  400     // Max gap between two taps for double-tap
#define SHAKE_THRESHOLD_G     2.5f    // Sustained acceleration for shake (in g)
#define SHAKE_DURATION_MS     600     // How long accel must stay high for shake
#define GESTURE_COOLDOWN_MS   400     // Cooldown after any gesture before next
#define EXERCISE_SHAKE_THRESHOLD_G 3.5f // Higher shake threshold during exercise

// ============================================================================
// FEATURE EXTRACTOR CONFIGURATION
// ============================================================================

// ============================================================================
// PHYSICAL BUTTON CONFIGURATION (GPIO push button)
// ============================================================================
/*
 * A single physical push button provides an alternative input method.
 * The button does NOT create new actions — it triggers the SAME GestureType
 * actions that the MPU6050 tap/shake system uses.
 *
 * Wiring: GPIO pin → button → GND (uses internal pull-up)
 *
 * ACTION MAPPING — change these to remap what each button interaction does.
 * Each value MUST be one of the existing GestureType enum values:
 *
 *   GESTURE_NONE        (0) = do nothing
 *   GESTURE_SINGLE_TAP  (1) = navigate / next item / acknowledge
 *   GESTURE_DOUBLE_TAP  (2) = select / confirm
 *   GESTURE_SHAKE       (3) = back / stop exercise / return to menu
 *
 * To change mappings later, just edit the three ACTION defines below.
 *
 * EXAMPLE ALTERNATIVE MAPPINGS:
 *
 *   "Button press = select, double = back, hold = next":
 *      BUTTON_PRESS_ACTION   GESTURE_DOUBLE_TAP
 *      BUTTON_DOUBLE_ACTION  GESTURE_SHAKE
 *      BUTTON_HOLD_ACTION    GESTURE_SINGLE_TAP
 *
 *   "Button press = next, hold = select, double = disabled":
 *      BUTTON_PRESS_ACTION   GESTURE_SINGLE_TAP
 *      BUTTON_DOUBLE_ACTION  GESTURE_NONE
 *      BUTTON_HOLD_ACTION    GESTURE_DOUBLE_TAP
 */

// --- Pin ---
#define BUTTON_PIN                4       // GPIO pin (wired to GND through button)

// --- Action mapping (what each interaction triggers) ---
#define BUTTON_PRESS_ACTION       GESTURE_SINGLE_TAP   // One quick press
#define BUTTON_DOUBLE_ACTION      GESTURE_DOUBLE_TAP   // Two quick presses
#define BUTTON_HOLD_ACTION        GESTURE_SHAKE         // Press and hold

// --- Timing (how interactions are recognized) ---
#define BUTTON_DEBOUNCE_MS        40      // Ignore bouncing shorter than this
#define BUTTON_DOUBLE_WINDOW_MS   400     // Max gap between presses for double-press
#define BUTTON_HOLD_MS            800     // Hold duration to trigger hold action

// ============================================================================
// FEATURE EXTRACTOR CONFIGURATION (continued)
// ============================================================================
#define FEATURE_WINDOW_SIZE   50   // Sliding window size (samples) for statistics
#define PEAK_MIN_PROMINENCE   0.3f // Minimum peak prominence (m/s²)
#define PEAK_MIN_DISTANCE_MS  150  // Minimum time between peaks (ms)

// ============================================================================
// AUTO-DETECTION CONFIGURATION
// ============================================================================
/*
 * The auto-detector continuously compares live sensor data against ALL
 * calibrated exercise profiles. It identifies which exercise the user is
 * performing and counts reps when a complete position sequence is detected.
 *
 * Flow: IDLE → CANDIDATE → TRACKING → COOLDOWN → TRACKING → ...
 *
 * Tuning guide:
 *   If FALSE DETECTIONS occur: INCREASE AUTO_MATCH_THRESHOLD or AUTO_CANDIDATE_SAMPLES
 *   If VALID EXERCISES are MISSED: DECREASE AUTO_MATCH_THRESHOLD
 *   If REPS are double-counted: INCREASE AUTO_COOLDOWN_MS
 *   If REPS are MISSED: DECREASE AUTO_TRANSITION_DEBOUNCE
 */

#define AUTO_MATCH_THRESHOLD        0.40f   // Min similarity (0-1) to consider a position matched
#define AUTO_CANDIDATE_SAMPLES      3       // P0 matches needed to identify exercise (decaying, not consecutive)
#define AUTO_TRANSITION_DEBOUNCE    2       // Matches needed for position transition (decaying, not consecutive)
#define AUTO_TRACKING_TIMEOUT_MS    15000   // Max time for one rep before reset to IDLE
#define AUTO_COOLDOWN_MS            300     // Brief pause after counting a rep
#define AUTO_IDLE_TIMEOUT_MS        5000    // Return to IDLE if no movement after last rep

// Rep timing tolerance — calibration stores the fastest/slowest calibration rep;
// live reps may naturally be faster or slower, so the window is widened.
#define REP_TIMING_MIN_FACTOR       0.3f    // live reps exclude rest time at P0, so they are shorter than calibration cycles
#define REP_TIMING_MAX_FACTOR       2.0f    // accept reps up to 200% of calibrated max duration
#define REP_TIMING_ABS_MIN_MS       150.0f  // never accept a rep faster than this (noise guard)

// ============================================================================
// POSITION MATCHING — Feature Weights
// ============================================================================
/*
 * Each live sensor reading is compared to a calibrated PositionProfile.
 * The overall similarity is a weighted average of per-feature similarities.
 *
 * Weights MUST sum to 1.0.
 *
 * Pitch is the primary discriminator for most exercises (standing vs squat vs lying).
 * AccelMag helps distinguish walking (periodic spikes) from static positions.
 * Roll and gyroMag are secondary features for disambiguation.
 */

#define POS_WEIGHT_PITCH            0.45f
#define POS_WEIGHT_ROLL             0.20f
#define POS_WEIGHT_ACCEL_MAG        0.30f
#define POS_WEIGHT_GYRO_MAG         0.05f

// ============================================================================
// POSITION MATCHING — Minimum Tolerance Floor
// ============================================================================
/*
 * Even if a user stands perfectly still during calibration (near-zero stddev),
 * the system enforces a minimum tolerance band. This prevents impossibly tight
 * position matching that would miss valid movements.
 *
 * These are the minimum half-width of the acceptance band around the mean.
 */

#define POS_MIN_PITCH_TOLERANCE     8.0f    // degrees (live pitch accepted within ±8° of calibrated mean at minimum)
#define POS_MIN_ROLL_TOLERANCE      3.0f    // degrees
#define POS_MIN_ACCEL_TOLERANCE     1.5f    // m/s²
#define POS_MIN_GYRO_TOLERANCE      15.0f   // deg/s (wide — gyro is unreliable for position matching)

// Range padding: how many stddevs beyond min/max to extend the acceptance range
#define POS_RANGE_PADDING_STDDEV    2.0f

// Periodic pipeline debug output interval
#define DEBUG_PIPELINE_MS           500     // Print pipeline state every N ms

// ============================================================================
// CALIBRATION CONFIGURATION
// ============================================================================
/*
 * Position-based calibration flow:
 *   1. User holds each position → system records sensor data (CAL_POSITION_RECORD_SEC)
 *   2. User performs N full reps → system extracts timing + dynamic P1 features
 *
 * For Walking and JJ: P1 is extracted dynamically from the movement recording.
 * For Squat and Sit-up: P1 can be captured statically (user holds the position).
 */

#define CAL_COUNTDOWN_SEC           3       // 3-2-1 countdown before recording
#define CAL_POSITION_RECORD_SEC     3       // Seconds to record each static position
#define CAL_WALKING_RECORD_SEC      12      // Seconds of walking to record
#define CAL_REP_RECORD_MAX_SEC      30      // Max seconds for rep recording phase
#define CAL_MAX_WAIT_SEC            60      // Max seconds waiting for user tap before timeout

// Target reps per exercise during calibration
#define CAL_REPS_WALKING            0       // Walking: timing extracted from walk sample, no explicit reps
#define CAL_REPS_SQUAT              3       // Squats to perform
#define CAL_REPS_JJ                 5       // Jumping jacks to perform
#define CAL_REPS_SITUP              3       // Sit-ups to perform

// Rep detection during calibration: distance-from-P0 threshold
#define CAL_REP_DETECT_FRACTION     0.4f    // Must reach 40% of max observed distance to count as a rep apex

// Position sample buffer size (must fit CAL_POSITION_RECORD_SEC * SENSOR_SAMPLE_RATE_HZ)
#define CAL_POS_BUFFER_SIZE         400     // 4 seconds at 100 Hz (with headroom)

// Dynamic recording buffer size (for walking / rep recording)
#define CAL_DYN_BUFFER_SIZE         3200    // ~32 seconds at 100 Hz (covers CAL_REP_RECORD_MAX_SEC)

// ============================================================================
// NVS (Non-Volatile Storage) CONFIGURATION
// ============================================================================
#define NVS_NAMESPACE         "fitness"      // Max 15 chars
#define NVS_KEY_PROFILE       "profile2"     // Changed key — forces re-calibration with new format
#define PROFILE_MAGIC         0xF175CA2FUL   // New magic — invalidates old data format

// ============================================================================
// TIMING CONSTANTS
// ============================================================================
#define SPLASH_DURATION_MS    2000     // Boot splash screen duration
#define MENU_SCROLL_DELAY_MS  200      // Minimum time between menu scrolls
#define STATE_TIMEOUT_MS      10000    // Reset detector if stuck in mid-state

// ============================================================================
// GRAVITY CONSTANT
// ============================================================================
#define GRAVITY_MS2           9.80665f // Standard gravity in m/s²
#define GRAVITY_G             1.0f     // 1g

#endif // CONFIG_H
