#ifndef CALIBRATION_MANAGER_H
#define CALIBRATION_MANAGER_H

#include "Types.h"
#include "Config.h"

class UserProfile;  // Will call profile->saveToNVS() after calibration

class CalibrationManager {
public:
    void begin(UserProfile* profile);
    void startCalibration(ExerciseType type);
    void update(const SensorFeatures& features, const FilteredData& filtered, GestureType gesture);
    bool isActive() const;
    CalibrationPhase getPhase() const;
    ExerciseType getActiveExercise() const;
    const char* getInstruction() const;
    int getProgress() const;          // 0-100
    int getDetectedReps() const;
    int getCurrentPosition() const;   // Which position being recorded (0 or 1)
    int getTotalPositions() const;     // Total positions for this exercise (always 2)

private:
    UserProfile* _profile;
    CalibrationPhase _phase;
    ExerciseType _activeExercise;
    int _currentPosIdx;        // 0 or 1
    int _totalPositions;       // Always 2
    bool _p1IsStatic;          // true for squat/sit-up, false for walking/JJ
    unsigned long _phaseStartTime;
    int _countdownValue;
    char _instruction[64];
    int _progress;
    
    // Static position recording buffers
    float _pitchBuf[CAL_POS_BUFFER_SIZE];
    float _rollBuf[CAL_POS_BUFFER_SIZE];
    float _accelMagBuf[CAL_POS_BUFFER_SIZE];
    float _gyroMagBuf[CAL_POS_BUFFER_SIZE];
    int _sampleCount;
    
    // Dynamic P1 extraction from reps
    static const int MAX_P1_SAMPLES = 100;
    float _p1PitchSamples[MAX_P1_SAMPLES];
    float _p1RollSamples[MAX_P1_SAMPLES];
    float _p1AccelSamples[MAX_P1_SAMPLES];
    float _p1GyroSamples[MAX_P1_SAMPLES];
    int _p1SampleCount;
    
    // Rep detection during recording
    int _detectedReps;
    int _targetReps;
    static const int MAX_REP_DURATIONS = 20;
    float _repDurations[MAX_REP_DURATIONS];
    int _repDurationCount;
    
    // Internal peak detection state for rep recording
    float _distFromP0;
    float _maxDistSeen;
    float _prevDist;
    bool _wasRising;
    bool _atPeak;
    unsigned long _repStartTime;
    unsigned long _lastPeakTime;
    
    // Temporary profile being built
    ExerciseProfile _tempProfile;
    
    void _computePositionStats(int posIdx);
    void _computeP1FromDynamic();
    void _finalizeProfile();
    void _setPhase(CalibrationPhase phase);
    void _setupExercise(ExerciseType type);
    const char* _getP0Instruction(ExerciseType type) const;
    const char* _getP1Instruction(ExerciseType type) const;
    const char* _getRepInstruction(ExerciseType type) const;
    int _getTargetReps(ExerciseType type) const;
    float _computeDistFromP0(const SensorFeatures& features) const;
};

#endif // CALIBRATION_MANAGER_H
