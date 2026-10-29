#include "CalibrationManager.h"
#include "UserProfile.h"
#include <math.h>

void CalibrationManager::begin(UserProfile* profile) {
    _profile = profile;
    _phase = CAL_PHASE_IDLE;
}

void CalibrationManager::startCalibration(ExerciseType type) {
    if (!_profile) return;
    _activeExercise = type;
    _setupExercise(type);
    _setPhase(CAL_PHASE_WAITING);
}

void CalibrationManager::update(const SensorFeatures& features, const FilteredData& filtered, GestureType gesture) {
    if (!isActive()) return;
    
    unsigned long now = millis();
    unsigned long elapsed = now - _phaseStartTime;

    switch (_phase) {
        case CAL_PHASE_WAITING:
            if (gesture == GESTURE_SINGLE_TAP) {
                _setPhase(CAL_PHASE_COUNTDOWN);
            } else if (gesture == GESTURE_SHAKE) {
                _setPhase(CAL_PHASE_FAILED);
            } else if (elapsed > CAL_MAX_WAIT_SEC * 1000UL) {
                _setPhase(CAL_PHASE_FAILED);
            }
            break;

        case CAL_PHASE_COUNTDOWN: {
            int newCount = CAL_COUNTDOWN_SEC - (elapsed / 1000);
            if (newCount <= 0) {
                if (_currentPosIdx < _totalPositions) {
                    _setPhase(CAL_PHASE_RECORDING_POS);
                } else {
                    _setPhase(CAL_PHASE_RECORDING_REPS);
                }
            } else if (newCount != _countdownValue) {
                _countdownValue = newCount;
                snprintf(_instruction, sizeof(_instruction), "Starting in %d...", _countdownValue);
            }
            if (gesture == GESTURE_SHAKE) {
                _setPhase(CAL_PHASE_FAILED);
            }
            break;
        }

        case CAL_PHASE_RECORDING_POS:
            if (gesture == GESTURE_SHAKE) {
                _setPhase(CAL_PHASE_FAILED);
                return;
            }
            if (_sampleCount < CAL_POS_BUFFER_SIZE) {
                _pitchBuf[_sampleCount] = features.pitch;
                _rollBuf[_sampleCount] = features.roll;
                _accelMagBuf[_sampleCount] = features.accelMagnitude;
                _gyroMagBuf[_sampleCount] = features.gyroMagnitude;
                _sampleCount++;
            }
            _progress = (elapsed * 100) / (CAL_POSITION_RECORD_SEC * 1000UL);
            if (_progress > 100) _progress = 100;
            
            if (elapsed >= CAL_POSITION_RECORD_SEC * 1000UL) {
                _computePositionStats(_currentPosIdx);
                
                DBG_CAL("[CAL] Position \"%s\" captured (%d samples)\n", _tempProfile.positions[_currentPosIdx].name, _sampleCount);
                DBG_CAL("  pitch: mean=%.2f std=%.2f range=[%.2f, %.2f]\n", 
                        _tempProfile.positions[_currentPosIdx].pitchMean, _tempProfile.positions[_currentPosIdx].pitchStdDev,
                        _tempProfile.positions[_currentPosIdx].pitchMin, _tempProfile.positions[_currentPosIdx].pitchMax);
                DBG_CAL("  roll:  mean=%.2f std=%.2f range=[%.2f, %.2f]\n", 
                        _tempProfile.positions[_currentPosIdx].rollMean, _tempProfile.positions[_currentPosIdx].rollStdDev,
                        _tempProfile.positions[_currentPosIdx].rollMin, _tempProfile.positions[_currentPosIdx].rollMax);
                DBG_CAL("  accel: mean=%.2f std=%.2f range=[%.2f, %.2f]\n", 
                        _tempProfile.positions[_currentPosIdx].accelMagMean, _tempProfile.positions[_currentPosIdx].accelMagStdDev,
                        _tempProfile.positions[_currentPosIdx].accelMagMin, _tempProfile.positions[_currentPosIdx].accelMagMax);
                DBG_CAL("  gyro:  mean=%.2f std=%.2f range=[%.2f, %.2f]\n", 
                        _tempProfile.positions[_currentPosIdx].gyroMagMean, _tempProfile.positions[_currentPosIdx].gyroMagStdDev,
                        _tempProfile.positions[_currentPosIdx].gyroMagMin, _tempProfile.positions[_currentPosIdx].gyroMagMax);
                
                _currentPosIdx++;
                
                if (_currentPosIdx == 1) {
                    if (_p1IsStatic) {
                        _setPhase(CAL_PHASE_WAITING);
                    } else {
                        _currentPosIdx++; // skip P1 static capture
                        _setPhase(CAL_PHASE_WAITING);
                    }
                } else if (_currentPosIdx >= _totalPositions) {
                    _setPhase(CAL_PHASE_WAITING);
                }
            }
            break;

        case CAL_PHASE_RECORDING_REPS: {
            if (gesture == GESTURE_SHAKE) {
                _setPhase(CAL_PHASE_FAILED);
                return;
            }
            
            _distFromP0 = _computeDistFromP0(features);
            if (_distFromP0 > _maxDistSeen) {
                _maxDistSeen = _distFromP0;
            }
            
            bool isRising = _distFromP0 > _prevDist;
            if (_wasRising && !isRising && _distFromP0 > _maxDistSeen * CAL_REP_DETECT_FRACTION) {
                _atPeak = true;
                if (!_p1IsStatic && _p1SampleCount < MAX_P1_SAMPLES) {
                    _p1PitchSamples[_p1SampleCount] = features.pitch;
                    _p1RollSamples[_p1SampleCount] = features.roll;
                    _p1AccelSamples[_p1SampleCount] = features.accelMagnitude;
                    _p1GyroSamples[_p1SampleCount] = features.gyroMagnitude;
                    _p1SampleCount++;
                }
                _lastPeakTime = now;
            }
            
            if (_atPeak && _distFromP0 < _maxDistSeen * 0.2f) {
                _atPeak = false;
                _detectedReps++;
                unsigned long dur = now - _repStartTime;
                if (_repDurationCount < MAX_REP_DURATIONS) {
                    _repDurations[_repDurationCount++] = dur;
                }
                _repStartTime = now;
                if (_activeExercise != EXERCISE_WALKING) {
                    snprintf(_instruction, sizeof(_instruction), "Go! (%d/%d reps)", _detectedReps, _targetReps);
                }
            }
            
            _wasRising = isRising;
            _prevDist = _distFromP0;
            
            if (_activeExercise == EXERCISE_WALKING) {
                _progress = (elapsed * 100) / (CAL_WALKING_RECORD_SEC * 1000UL);
                if (elapsed >= CAL_WALKING_RECORD_SEC * 1000UL) {
                    _setPhase(CAL_PHASE_PROCESSING);
                }
            } else {
                _progress = (_detectedReps * 100) / (_targetReps > 0 ? _targetReps : 1);
                if (_detectedReps >= _targetReps || elapsed >= CAL_REP_RECORD_MAX_SEC * 1000UL) {
                    _setPhase(CAL_PHASE_PROCESSING);
                }
            }
            if (_progress > 100) _progress = 100;
            break;
        }

        case CAL_PHASE_PROCESSING:
            if (!_p1IsStatic) {
                _computeP1FromDynamic();
            }
            _finalizeProfile();
            _setPhase(CAL_PHASE_COMPLETE);
            break;

        case CAL_PHASE_COMPLETE:
            if (gesture == GESTURE_SINGLE_TAP || gesture == GESTURE_SHAKE) {
                _phase = CAL_PHASE_IDLE;
            }
            break;
            
        case CAL_PHASE_FAILED:
            if (gesture == GESTURE_SINGLE_TAP || gesture == GESTURE_SHAKE) {
                _phase = CAL_PHASE_IDLE;
            }
            break;

        default:
            break;
    }
}

bool CalibrationManager::isActive() const {
    return _phase != CAL_PHASE_IDLE;
}

CalibrationPhase CalibrationManager::getPhase() const {
    return _phase;
}

ExerciseType CalibrationManager::getActiveExercise() const {
    return _activeExercise;
}

const char* CalibrationManager::getInstruction() const {
    return _instruction;
}

int CalibrationManager::getProgress() const {
    return _progress;
}

int CalibrationManager::getDetectedReps() const {
    return _detectedReps;
}

int CalibrationManager::getCurrentPosition() const {
    return _currentPosIdx;
}

int CalibrationManager::getTotalPositions() const {
    return _totalPositions;
}

void CalibrationManager::_computePositionStats(int posIdx) {
    if (_sampleCount == 0) return;
    
    PositionProfile& pos = _tempProfile.positions[posIdx];
    
    float sumPitch = 0, sumRoll = 0, sumAccel = 0, sumGyro = 0;
    float minPitch = _pitchBuf[0], maxPitch = _pitchBuf[0];
    float minRoll = _rollBuf[0], maxRoll = _rollBuf[0];
    float minAccel = _accelMagBuf[0], maxAccel = _accelMagBuf[0];
    float minGyro = _gyroMagBuf[0], maxGyro = _gyroMagBuf[0];
    
    for (int i = 0; i < _sampleCount; i++) {
        sumPitch += _pitchBuf[i];
        if (_pitchBuf[i] < minPitch) minPitch = _pitchBuf[i];
        if (_pitchBuf[i] > maxPitch) maxPitch = _pitchBuf[i];
        
        sumRoll += _rollBuf[i];
        if (_rollBuf[i] < minRoll) minRoll = _rollBuf[i];
        if (_rollBuf[i] > maxRoll) maxRoll = _rollBuf[i];
        
        sumAccel += _accelMagBuf[i];
        if (_accelMagBuf[i] < minAccel) minAccel = _accelMagBuf[i];
        if (_accelMagBuf[i] > maxAccel) maxAccel = _accelMagBuf[i];
        
        sumGyro += _gyroMagBuf[i];
        if (_gyroMagBuf[i] < minGyro) minGyro = _gyroMagBuf[i];
        if (_gyroMagBuf[i] > maxGyro) maxGyro = _gyroMagBuf[i];
    }
    
    pos.pitchMean = sumPitch / _sampleCount;
    pos.rollMean = sumRoll / _sampleCount;
    pos.accelMagMean = sumAccel / _sampleCount;
    pos.gyroMagMean = sumGyro / _sampleCount;
    
    float varPitch = 0, varRoll = 0, varAccel = 0, varGyro = 0;
    for (int i = 0; i < _sampleCount; i++) {
        varPitch += (_pitchBuf[i] - pos.pitchMean) * (_pitchBuf[i] - pos.pitchMean);
        varRoll += (_rollBuf[i] - pos.rollMean) * (_rollBuf[i] - pos.rollMean);
        varAccel += (_accelMagBuf[i] - pos.accelMagMean) * (_accelMagBuf[i] - pos.accelMagMean);
        varGyro += (_gyroMagBuf[i] - pos.gyroMagMean) * (_gyroMagBuf[i] - pos.gyroMagMean);
    }
    
    pos.pitchStdDev = sqrt(varPitch / _sampleCount);
    pos.rollStdDev = sqrt(varRoll / _sampleCount);
    pos.accelMagStdDev = sqrt(varAccel / _sampleCount);
    pos.gyroMagStdDev = sqrt(varGyro / _sampleCount);
    
    pos.pitchMin = minPitch; pos.pitchMax = maxPitch;
    pos.rollMin = minRoll; pos.rollMax = maxRoll;
    pos.accelMagMin = minAccel; pos.accelMagMax = maxAccel;
    pos.gyroMagMin = minGyro; pos.gyroMagMax = maxGyro;
    
    pos.sampleCount = _sampleCount;
    pos.isValid = true;
}

void CalibrationManager::_computeP1FromDynamic() {
    if (_p1SampleCount == 0) return;
    
    PositionProfile& pos = _tempProfile.positions[1];
    
    float sumPitch = 0, sumRoll = 0, sumAccel = 0, sumGyro = 0;
    float minPitch = _p1PitchSamples[0], maxPitch = _p1PitchSamples[0];
    float minRoll = _p1RollSamples[0], maxRoll = _p1RollSamples[0];
    float minAccel = _p1AccelSamples[0], maxAccel = _p1AccelSamples[0];
    float minGyro = _p1GyroSamples[0], maxGyro = _p1GyroSamples[0];
    
    for (int i = 0; i < _p1SampleCount; i++) {
        sumPitch += _p1PitchSamples[i];
        if (_p1PitchSamples[i] < minPitch) minPitch = _p1PitchSamples[i];
        if (_p1PitchSamples[i] > maxPitch) maxPitch = _p1PitchSamples[i];
        
        sumRoll += _p1RollSamples[i];
        if (_p1RollSamples[i] < minRoll) minRoll = _p1RollSamples[i];
        if (_p1RollSamples[i] > maxRoll) maxRoll = _p1RollSamples[i];
        
        sumAccel += _p1AccelSamples[i];
        if (_p1AccelSamples[i] < minAccel) minAccel = _p1AccelSamples[i];
        if (_p1AccelSamples[i] > maxAccel) maxAccel = _p1AccelSamples[i];
        
        sumGyro += _p1GyroSamples[i];
        if (_p1GyroSamples[i] < minGyro) minGyro = _p1GyroSamples[i];
        if (_p1GyroSamples[i] > maxGyro) maxGyro = _p1GyroSamples[i];
    }
    
    pos.pitchMean = sumPitch / _p1SampleCount;
    pos.rollMean = sumRoll / _p1SampleCount;
    pos.accelMagMean = sumAccel / _p1SampleCount;
    pos.gyroMagMean = sumGyro / _p1SampleCount;
    
    float varPitch = 0, varRoll = 0, varAccel = 0, varGyro = 0;
    for (int i = 0; i < _p1SampleCount; i++) {
        varPitch += (_p1PitchSamples[i] - pos.pitchMean) * (_p1PitchSamples[i] - pos.pitchMean);
        varRoll += (_p1RollSamples[i] - pos.rollMean) * (_p1RollSamples[i] - pos.rollMean);
        varAccel += (_p1AccelSamples[i] - pos.accelMagMean) * (_p1AccelSamples[i] - pos.accelMagMean);
        varGyro += (_p1GyroSamples[i] - pos.gyroMagMean) * (_p1GyroSamples[i] - pos.gyroMagMean);
    }
    
    pos.pitchStdDev = sqrt(varPitch / _p1SampleCount);
    pos.rollStdDev = sqrt(varRoll / _p1SampleCount);
    pos.accelMagStdDev = sqrt(varAccel / _p1SampleCount);
    pos.gyroMagStdDev = sqrt(varGyro / _p1SampleCount);
    
    pos.pitchMin = minPitch; pos.pitchMax = maxPitch;
    pos.rollMin = minRoll; pos.rollMax = maxRoll;
    pos.accelMagMin = minAccel; pos.accelMagMax = maxAccel;
    pos.gyroMagMin = minGyro; pos.gyroMagMax = maxGyro;
    
    pos.sampleCount = _p1SampleCount;
    pos.isValid = true;
    
    DBG_CAL("[CAL] Position \"%s\" extracted dynamically (%d samples)\n", pos.name, _p1SampleCount);
    DBG_CAL("  pitch: mean=%.2f std=%.2f range=[%.2f, %.2f]\n", pos.pitchMean, pos.pitchStdDev, pos.pitchMin, pos.pitchMax);
    DBG_CAL("  accel: mean=%.2f std=%.2f range=[%.2f, %.2f]\n", pos.accelMagMean, pos.accelMagStdDev, pos.accelMagMin, pos.accelMagMax);
}

void CalibrationManager::_finalizeProfile() {
    float minD = 15000.0f, maxD = 200.0f, sumD = 0;
    for (int i = 0; i < _repDurationCount; i++) {
        if (_repDurations[i] < minD) minD = _repDurations[i];
        if (_repDurations[i] > maxD) maxD = _repDurations[i];
        sumD += _repDurations[i];
    }
    
    if (_repDurationCount > 0) {
        _tempProfile.avgRepDurationMs = sumD / _repDurationCount;
        _tempProfile.minRepDurationMs = minD < 200.0f ? 200.0f : minD;
        _tempProfile.maxRepDurationMs = maxD > 15000.0f ? 15000.0f : maxD;
    } else {
        _tempProfile.avgRepDurationMs = 1000.0f;
        _tempProfile.minRepDurationMs = 500.0f;
        _tempProfile.maxRepDurationMs = 3000.0f;
    }
    
    _tempProfile.isCalibrated = true;
    _profile->data.exercises[exerciseIndex(_activeExercise)] = _tempProfile;
    _profile->saveToNVS();
    
    DBG_CAL("=== %s CALIBRATION COMPLETE ===\n", _tempProfile.name);
    DBG_CAL("P0 \"%s\": pitch=%.2f±%.2f  accel=%.2f±%.2f\n", 
            _tempProfile.positions[0].name, _tempProfile.positions[0].pitchMean, _tempProfile.positions[0].pitchStdDev,
            _tempProfile.positions[0].accelMagMean, _tempProfile.positions[0].accelMagStdDev);
    DBG_CAL("P1 \"%s\": pitch=%.2f±%.2f  accel=%.2f±%.2f\n", 
            _tempProfile.positions[1].name, _tempProfile.positions[1].pitchMean, _tempProfile.positions[1].pitchStdDev,
            _tempProfile.positions[1].accelMagMean, _tempProfile.positions[1].accelMagStdDev);
    DBG_CAL("Timing: min=%.0fms avg=%.0fms max=%.0fms\n", 
            _tempProfile.minRepDurationMs, _tempProfile.avgRepDurationMs, _tempProfile.maxRepDurationMs);
}

void CalibrationManager::_setPhase(CalibrationPhase phase) {
    _phase = phase;
    _phaseStartTime = millis();
    _progress = 0;

    switch (phase) {
        case CAL_PHASE_WAITING:
            if (_currentPosIdx == 0) {
                snprintf(_instruction, sizeof(_instruction), "%s", _getP0Instruction(_activeExercise));
            } else if (_currentPosIdx == 1) {
                snprintf(_instruction, sizeof(_instruction), "%s", _getP1Instruction(_activeExercise));
            } else {
                snprintf(_instruction, sizeof(_instruction), "%s", _getRepInstruction(_activeExercise));
            }
            break;
            
        case CAL_PHASE_COUNTDOWN:
            _countdownValue = CAL_COUNTDOWN_SEC;
            snprintf(_instruction, sizeof(_instruction), "Starting in %d...", _countdownValue);
            break;
            
        case CAL_PHASE_RECORDING_POS:
            snprintf(_instruction, sizeof(_instruction), "Recording %s...", _tempProfile.positions[_currentPosIdx].name);
            _sampleCount = 0;
            break;
            
        case CAL_PHASE_RECORDING_REPS:
            if (_activeExercise == EXERCISE_WALKING) {
                snprintf(_instruction, sizeof(_instruction), "Walking...");
            } else {
                snprintf(_instruction, sizeof(_instruction), "Go! (0/%d reps)", _targetReps);
            }
            _detectedReps = 0;
            _repDurationCount = 0;
            _p1SampleCount = 0;
            _distFromP0 = 0;
            _maxDistSeen = 0;
            _prevDist = 0;
            _wasRising = false;
            _atPeak = false;
            _repStartTime = millis();
            _lastPeakTime = 0;
            break;
            
        case CAL_PHASE_PROCESSING:
            snprintf(_instruction, sizeof(_instruction), "Processing...");
            break;
            
        case CAL_PHASE_COMPLETE:
            snprintf(_instruction, sizeof(_instruction), "%s calibrated! Tap to exit.", _tempProfile.name);
            break;
            
        case CAL_PHASE_FAILED:
            snprintf(_instruction, sizeof(_instruction), "Failed/Cancelled. Tap to exit.");
            break;
            
        default:
            break;
    }
}

void CalibrationManager::_setupExercise(ExerciseType type) {
    _tempProfile.type = type;
    _tempProfile.positionCount = 2;
    _totalPositions = 2;
    _currentPosIdx = 0;
    _targetReps = _getTargetReps(type);
    
    switch (type) {
        case EXERCISE_WALKING:
            snprintf(_tempProfile.name, sizeof(_tempProfile.name), "Walking");
            snprintf(_tempProfile.positions[0].name, sizeof(_tempProfile.positions[0].name), "Standing");
            snprintf(_tempProfile.positions[1].name, sizeof(_tempProfile.positions[1].name), "Step Peak");
            _p1IsStatic = false;
            break;
        case EXERCISE_SQUAT:
            snprintf(_tempProfile.name, sizeof(_tempProfile.name), "Squat");
            snprintf(_tempProfile.positions[0].name, sizeof(_tempProfile.positions[0].name), "Standing");
            snprintf(_tempProfile.positions[1].name, sizeof(_tempProfile.positions[1].name), "Squat Down");
            _p1IsStatic = true;
            break;
        case EXERCISE_JUMPING_JACK:
            snprintf(_tempProfile.name, sizeof(_tempProfile.name), "Jumping Jack");
            snprintf(_tempProfile.positions[0].name, sizeof(_tempProfile.positions[0].name), "Ready");
            snprintf(_tempProfile.positions[1].name, sizeof(_tempProfile.positions[1].name), "Jump Peak");
            _p1IsStatic = false;
            break;
        case EXERCISE_SIT_UP:
            snprintf(_tempProfile.name, sizeof(_tempProfile.name), "Sit-up");
            snprintf(_tempProfile.positions[0].name, sizeof(_tempProfile.positions[0].name), "Lying Flat");
            snprintf(_tempProfile.positions[1].name, sizeof(_tempProfile.positions[1].name), "Sitting Up");
            _p1IsStatic = true;
            break;
        default:
            snprintf(_tempProfile.name, sizeof(_tempProfile.name), "Unknown");
            break;
    }
}

const char* CalibrationManager::_getP0Instruction(ExerciseType type) const {
    switch (type) {
        case EXERCISE_WALKING: return "Stand still, tap to start";
        case EXERCISE_SQUAT: return "Stand upright, tap to start";
        case EXERCISE_JUMPING_JACK: return "Stand still (arms down), tap";
        case EXERCISE_SIT_UP: return "Lie flat on back, tap";
        default: return "Get ready, tap to start";
    }
}

const char* CalibrationManager::_getP1Instruction(ExerciseType type) const {
    switch (type) {
        case EXERCISE_SQUAT: return "Squat down fully, hold, tap";
        case EXERCISE_SIT_UP: return "Sit up fully, hold, tap";
        default: return "Hold position, tap to start";
    }
}

const char* CalibrationManager::_getRepInstruction(ExerciseType type) const {
    switch (type) {
        case EXERCISE_WALKING: return "Walk normally, tap to start";
        case EXERCISE_SQUAT: return "Do 3 squats, tap to start";
        case EXERCISE_JUMPING_JACK: return "Do 5 jumping jacks, tap to start";
        case EXERCISE_SIT_UP: return "Do 3 sit-ups, tap to start";
        default: return "Do reps, tap to start";
    }
}

int CalibrationManager::_getTargetReps(ExerciseType type) const {
    switch (type) {
        case EXERCISE_WALKING: return CAL_REPS_WALKING;
        case EXERCISE_SQUAT: return CAL_REPS_SQUAT;
        case EXERCISE_JUMPING_JACK: return CAL_REPS_JJ;
        case EXERCISE_SIT_UP: return CAL_REPS_SITUP;
        default: return 3;
    }
}

float CalibrationManager::_computeDistFromP0(const SensorFeatures& features) const {
    const PositionProfile& p0 = _tempProfile.positions[0];
    
    float pitchTerm = (features.pitch - p0.pitchMean) / (p0.pitchStdDev > 1.0f ? p0.pitchStdDev : 1.0f);
    float accelTerm = (features.accelMagnitude - p0.accelMagMean) / (p0.accelMagStdDev > 0.5f ? p0.accelMagStdDev : 0.5f);
    
    return sqrt(POS_WEIGHT_PITCH * pitchTerm * pitchTerm + 
                POS_WEIGHT_ACCEL_MAG * accelTerm * accelTerm);
}
