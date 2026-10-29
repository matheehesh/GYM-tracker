#include "UserProfile.h"
#include <Preferences.h>
#include <string.h>
#include <stdio.h>

Preferences prefs;

void UserProfile::begin() {
    if (!loadFromNVS()) {
        setDefaults();
        saveToNVS();
    }
}

void UserProfile::setDefaults() {
    memset(&data, 0, sizeof(data));
    
    // Set exercise types and names
    data.exercises[0].type = EXERCISE_WALKING;
    strncpy(data.exercises[0].name, "Walking", 15);
    data.exercises[0].positionCount = 2;
    strncpy(data.exercises[0].positions[0].name, "Standing", 15);
    strncpy(data.exercises[0].positions[1].name, "Step Peak", 15);
    data.exercises[0].isCalibrated = false;
    data.exercises[0].minRepDurationMs = 200;
    data.exercises[0].maxRepDurationMs = 2000;
    data.exercises[0].avgRepDurationMs = 550;
    
    data.exercises[1].type = EXERCISE_SQUAT;
    strncpy(data.exercises[1].name, "Squat", 15);
    data.exercises[1].positionCount = 2;
    strncpy(data.exercises[1].positions[0].name, "Standing", 15);
    strncpy(data.exercises[1].positions[1].name, "Squat Down", 15);
    data.exercises[1].isCalibrated = false;
    data.exercises[1].minRepDurationMs = 800;
    data.exercises[1].maxRepDurationMs = 8000;
    data.exercises[1].avgRepDurationMs = 2000;
    
    data.exercises[2].type = EXERCISE_JUMPING_JACK;
    strncpy(data.exercises[2].name, "Jump Jack", 15);
    data.exercises[2].positionCount = 2;
    strncpy(data.exercises[2].positions[0].name, "Ready", 15);
    strncpy(data.exercises[2].positions[1].name, "Jump Peak", 15);
    data.exercises[2].isCalibrated = false;
    data.exercises[2].minRepDurationMs = 400;
    data.exercises[2].maxRepDurationMs = 2000;
    data.exercises[2].avgRepDurationMs = 800;
    
    data.exercises[3].type = EXERCISE_SIT_UP;
    strncpy(data.exercises[3].name, "Sit-up", 15);
    data.exercises[3].positionCount = 2;
    strncpy(data.exercises[3].positions[0].name, "Lying Flat", 15);
    strncpy(data.exercises[3].positions[1].name, "Sitting Up", 15);
    data.exercises[3].isCalibrated = false;
    data.exercises[3].minRepDurationMs = 1000;
    data.exercises[3].maxRepDurationMs = 10000;
    data.exercises[3].avgRepDurationMs = 2500;
    
    for (int i = 0; i < MAX_EXERCISES; i++) {
        data.exerciseCounts[i] = 0;
    }
    
    data.magic = PROFILE_MAGIC;
}

bool UserProfile::loadFromNVS() {
    prefs.begin(NVS_NAMESPACE, true);
    size_t len = prefs.getBytesLength(NVS_KEY_PROFILE);
    if (len != sizeof(data)) { prefs.end(); return false; }
    prefs.getBytes(NVS_KEY_PROFILE, &data, sizeof(data));
    prefs.end();
    return (data.magic == PROFILE_MAGIC);
}

bool UserProfile::saveToNVS() {
    data.magic = PROFILE_MAGIC;
    prefs.begin(NVS_NAMESPACE, false);
    size_t written = prefs.putBytes(NVS_KEY_PROFILE, &data, sizeof(data));
    prefs.end();
    return (written == sizeof(data));
}

void UserProfile::resetAll() {
    setDefaults();
    saveToNVS();
}

void UserProfile::resetExercise(ExerciseType type) {
    int idx = exerciseIndex(type);
    if (idx >= 0 && idx < MAX_EXERCISES) {
        data.exercises[idx].isCalibrated = false;
        saveToNVS();
    }
}

bool UserProfile::isCalibrated(ExerciseType type) const {
    int idx = exerciseIndex(type);
    if (idx >= 0 && idx < MAX_EXERCISES) {
        return data.exercises[idx].isCalibrated;
    }
    return false;
}

bool UserProfile::isAnyCalibrated() const {
    for (int i = 0; i < MAX_EXERCISES; i++) {
        if (data.exercises[i].isCalibrated) return true;
    }
    return false;
}

void UserProfile::printCalibration() const {
    for (int i = 0; i < MAX_EXERCISES; i++) {
        printExerciseProfile(data.exercises[i].type);
    }
}

void UserProfile::printExerciseProfile(ExerciseType type) const {
    int idx = exerciseIndex(type);
    if (idx < 0 || idx >= MAX_EXERCISES) return;
    
    const ExerciseProfile& ex = data.exercises[idx];
    Serial.printf("=== %s ===\n", ex.name);
    Serial.printf("Calibrated: %s\n", ex.isCalibrated ? "YES" : "NO");
    
    for (int p = 0; p < ex.positionCount; p++) {
        const PositionProfile& pos = ex.positions[p];
        Serial.printf("P%d \"%s\":\n", p, pos.name);
        Serial.printf("  pitch: %.1f ± %.1f [%.1f, %.1f]\n", pos.pitchMean, pos.pitchStdDev, pos.pitchMin, pos.pitchMax);
        Serial.printf("  roll:  %.1f ± %.1f [%.1f, %.1f]\n", pos.rollMean, pos.rollStdDev, pos.rollMin, pos.rollMax);
        Serial.printf("  accel: %.2f ± %.2f [%.2f, %.2f]\n", pos.accelMagMean, pos.accelMagStdDev, pos.accelMagMin, pos.accelMagMax);
        Serial.printf("  gyro:  %.1f ± %.1f [%.1f, %.1f]\n", pos.gyroMagMean, pos.gyroMagStdDev, pos.gyroMagMin, pos.gyroMagMax);
    }
    Serial.printf("Timing: %d-%dms (avg %dms)\n", (int)ex.minRepDurationMs, (int)ex.maxRepDurationMs, (int)ex.avgRepDurationMs);
}
