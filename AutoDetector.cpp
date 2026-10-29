#include "AutoDetector.h"
#include <math.h>

// ============================================================================
// Helper function implementations
// ============================================================================

const char* exerciseTypeName(ExerciseType t) {
    switch (t) {
        case EXERCISE_WALKING: return "Walking";
        case EXERCISE_SQUAT: return "Squat";
        case EXERCISE_JUMPING_JACK: return "Jumping Jack";
        case EXERCISE_SIT_UP: return "Sit Up";
        default: return "Unknown";
    }
}

const char* autoDetectStateName(AutoDetectState s) {
    switch (s) {
        case AUTO_IDLE: return "IDLE";
        case AUTO_CANDIDATE: return "CANDIDATE";
        case AUTO_TRACKING: return "TRACKING";
        case AUTO_COOLDOWN: return "COOLDOWN";
        default: return "UNKNOWN";
    }
}

const char* calPhaseName(CalibrationPhase p) {
    switch (p) {
        case CAL_PHASE_IDLE: return "IDLE";
        case CAL_PHASE_WAITING: return "WAITING";
        case CAL_PHASE_COUNTDOWN: return "COUNTDOWN";
        case CAL_PHASE_RECORDING_POS: return "RECORDING_POS";
        case CAL_PHASE_RECORDING_REPS: return "RECORDING_REPS";
        case CAL_PHASE_PROCESSING: return "PROCESSING";
        case CAL_PHASE_COMPLETE: return "COMPLETE";
        case CAL_PHASE_FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// AutoDetector implementation
// ============================================================================

void AutoDetector::begin(const UserProfileData* profile) {
    _profile = profile;
    _lastDebugPrint = 0;
    _state = AUTO_IDLE;
    resetCounts();
    reset();
}

void AutoDetector::reset() {
    _transitionTo(AUTO_IDLE);
    _detectedExercise = EXERCISE_UNKNOWN;
    _sequenceStep = 0;
    _confidence = 0.0f;
    for (int i = 0; i < MAX_EXERCISES; i++) {
        _candidateCounts[i] = 0;
    }
    _p1MatchCount = 0;
    _p0ReturnCount = 0;

    unsigned long now = millis();
    _candidateStartTime = now;
    _trackingStartTime = now;
    _repStartTime = now;
    _cooldownStartTime = now;
    _lastActiveTime = now;
    _lastP0MatchTime = now;
}

void AutoDetector::setProfile(const UserProfileData* profile) {
    _profile = profile;
}

AutoDetectState AutoDetector::getState() const { return _state; }
ExerciseType AutoDetector::getDetectedExercise() const { return _detectedExercise; }
int AutoDetector::getCurrentSequenceStep() const { return _sequenceStep; }
float AutoDetector::getConfidence() const { return _confidence; }
uint32_t AutoDetector::getRepCount(ExerciseType type) const { return _repCounts[exerciseIndex(type)]; }

uint32_t AutoDetector::getTotalReps() const {
    uint32_t total = 0;
    for (int i = 0; i < MAX_EXERCISES; i++) {
        total += _repCounts[i];
    }
    return total;
}

void AutoDetector::resetCounts() {
    for (int i = 0; i < MAX_EXERCISES; i++) {
        _repCounts[i] = 0;
    }
}

void AutoDetector::_transitionTo(AutoDetectState newState) {
    if (_state != newState) {
        DBG_AUTO("[AUTO] %s -> %s\n", autoDetectStateName(_state), autoDetectStateName(newState));
    }
    _state = newState;
}

// ============================================================================
// Feature similarity: 1.0 at mean, linear decay to 0.0 at 3ÃÆ’
// ============================================================================
float AutoDetector::_computeFeatureSimilarity(float value, float mean, float stdDev, float minTolerance) const {
    // Enforce minimum tolerance so near-zero calibration stddev doesn't make matching impossible
    float effectiveStdDev = stdDev > (minTolerance / 3.0f) ? stdDev : (minTolerance / 3.0f);
    float zScore = fabsf(value - mean) / effectiveStdDev;
    float sim = 1.0f - (zScore / 3.0f);
    return sim > 0.0f ? sim : 0.0f;
}

// ============================================================================
// A position MATCHES only if:
//   1. weighted similarity >= AUTO_MATCH_THRESHOLD, AND
//   2. the discriminating features (pitch, accelMag) lie inside the learned
//      (padded) range.
// Without (2), standing (pitch ~5°) scored ~0.55 against "Squat Down"
// (pitch ~45°) purely from roll/accel/gyro, so P0 and P1 matched at once.
// ============================================================================
bool AutoDetector::_isMatch(const PositionMatchResult& r) {
    return r.isWithinRange && r.similarity >= AUTO_MATCH_THRESHOLD;
}

// ============================================================================
// Position matching: weighted similarity of pitch, roll, accelMag, gyroMag
// ============================================================================
PositionMatchResult AutoDetector::_matchPosition(const SensorFeatures& features, const PositionProfile& pos) const {
    if (!pos.isValid) return {0.0f, false};

    float pitchSim = _computeFeatureSimilarity(features.pitch, pos.pitchMean, pos.pitchStdDev, POS_MIN_PITCH_TOLERANCE);
    float rollSim = _computeFeatureSimilarity(features.roll, pos.rollMean, pos.rollStdDev, POS_MIN_ROLL_TOLERANCE);
    float accelSim = _computeFeatureSimilarity(features.accelMagnitude, pos.accelMagMean, pos.accelMagStdDev, POS_MIN_ACCEL_TOLERANCE);
    float gyroSim = _computeFeatureSimilarity(features.gyroMagnitude, pos.gyroMagMean, pos.gyroMagStdDev, POS_MIN_GYRO_TOLERANCE);

    float similarity = pitchSim * POS_WEIGHT_PITCH + rollSim * POS_WEIGHT_ROLL
                     + accelSim * POS_WEIGHT_ACCEL_MAG + gyroSim * POS_WEIGHT_GYRO_MAG;

    // Tolerance gate: the discriminating features (pitch = body orientation,
    // accelMag = impact/bounce) must EACH lie inside the learned tolerance band
    // (within 3 effective σ of the calibrated mean, never tighter than
    // POS_MIN_*_TOLERANCE). Roll and gyro only contribute to the score.
    bool inRange = (pitchSim > 0.0f) && (accelSim > 0.0f);

    return {similarity, inRange};
}

// ============================================================================
// Periodic debug: print pipeline state every DEBUG_PIPELINE_MS
// ============================================================================
void AutoDetector::_printDebug(const SensorFeatures& features) {
    unsigned long now = millis();
    if (now - _lastDebugPrint < DEBUG_PIPELINE_MS) return;
    _lastDebugPrint = now;

    // [FEATURES] Live sensor features
    DBG_AUTO("[FEATURES] pitch=%.1f roll=%.1f accelMag=%.2f gyroMag=%.1f\n",
             features.pitch, features.roll, features.accelMagnitude, features.gyroMagnitude);

    // [POSITION MATCH] Score against each calibrated exercise
    for (int i = 0; i < MAX_EXERCISES; i++) {
        const ExerciseProfile& ex = _profile->exercises[i];
        if (!ex.isCalibrated) continue;

        PositionMatchResult p0 = _matchPosition(features, ex.positions[0]);
        PositionMatchResult p1 = _matchPosition(features, ex.positions[1]);

        DBG_AUTO("[POSITION MATCH] %s: P0=%.2f(%s) P1=%.2f(%s) cand=%d\n",
                 exerciseTypeName(ex.type),
                 p0.similarity, _isMatch(p0) ? "Y" : "n",
                 p1.similarity, _isMatch(p1) ? "Y" : "n",
                 _candidateCounts[i]);
    }

    // [STATE] Current detector state
    if (_detectedExercise != EXERCISE_UNKNOWN) {
        DBG_AUTO("[STATE] %s exercise=%s step=%d conf=%.0f%% p1match=%d p0ret=%d\n",
                 autoDetectStateName(_state), exerciseTypeName(_detectedExercise),
                 _sequenceStep, _confidence, _p1MatchCount, _p0ReturnCount);
    } else {
        DBG_AUTO("[STATE] %s\n", autoDetectStateName(_state));
    }

    // [REPETITION COUNT]
    bool anyCounts = false;
    for (int i = 0; i < MAX_EXERCISES; i++) {
        if (_repCounts[i] > 0) {
            DBG_AUTO("[REPETITION COUNT] %s=%u\n", exerciseTypeName(exerciseFromIndex(i)), (unsigned int)_repCounts[i]);
            anyCounts = true;
        }
    }
    if (!anyCounts) {
        DBG_AUTO("[REPETITION COUNT] (none)\n");
    }
    DBG_AUTO("---\n");
}

// ============================================================================
// Main update Ã¢â‚¬â€ called every sensor cycle (~100Hz)
// ============================================================================
void AutoDetector::update(const SensorFeatures& features) {
    if (!_profile) return;

    unsigned long now = millis();

    // Periodic debug output
    _printDebug(features);

    switch (_state) {

        // ==================================================================
        // IDLE: Scan all calibrated exercises for P0 match
        // ==================================================================
        case AUTO_IDLE: {
            int bestIdx = -1;
            float bestSim = 0.0f;

            for (int i = 0; i < MAX_EXERCISES; i++) {
                const ExerciseProfile& ex = _profile->exercises[i];
                if (!ex.isCalibrated) continue;

                PositionMatchResult match = _matchPosition(features, ex.positions[0]);
                if (_isMatch(match)) {
                    // DECAYING counter: increment on match (capped)
                    if (_candidateCounts[i] < AUTO_CANDIDATE_SAMPLES * 3) {
                        _candidateCounts[i]++;
                    }
                    _lastActiveTime = now;

                    if (_candidateCounts[i] >= AUTO_CANDIDATE_SAMPLES && match.similarity > bestSim) {
                        bestSim = match.similarity;
                        bestIdx = i;
                    }
                } else {
                    // SOFT DECAY: decrement by 1 instead of hard reset to 0
                    if (_candidateCounts[i] > 0) {
                        _candidateCounts[i]--;
                    }
                }
            }

            // If any exercise reached the candidate threshold, transition
            if (bestIdx >= 0) {
                _detectedExercise = _profile->exercises[bestIdx].type;
                _candidateStartTime = now;
                _confidence = bestSim * 100.0f;
                _p1MatchCount = 0;
                DBG_AUTO("[DETECTED EXERCISE] CANDIDATE: %s (sim=%.2f, count=%d)\n",
                         exerciseTypeName(_detectedExercise), bestSim, _candidateCounts[bestIdx]);
                _transitionTo(AUTO_CANDIDATE);
            }
            break;
        }

        // ==================================================================
        // CANDIDATE: P0 confirmed Ã¢â‚¬â€ wait for P1 to identify the specific exercise
        // ==================================================================
        case AUTO_CANDIDATE: {
            if (now - _candidateStartTime > AUTO_TRACKING_TIMEOUT_MS) {
                DBG_AUTO("[STATE] CANDIDATE TIMEOUT -> IDLE\n");
                reset();
                break;
            }

            // Remember the last moment the user was still at the start position.
            // This becomes the start time of the first repetition.
            int candIdx = exerciseIndex(_detectedExercise);
            if (candIdx >= 0 && candIdx < MAX_EXERCISES &&
                _isMatch(_matchPosition(features, _profile->exercises[candIdx].positions[0]))) {
                _lastP0MatchTime = now;
            }

            // Find the BEST P1 match among exercises whose P0 matched
            int bestIdx = -1;
            float bestSim = 0.0f;
            for (int i = 0; i < MAX_EXERCISES; i++) {
                const ExerciseProfile& ex = _profile->exercises[i];
                if (!ex.isCalibrated) continue;
                if (_candidateCounts[i] < 1) continue;   // P0 never matched for this exercise

                PositionMatchResult match = _matchPosition(features, ex.positions[1]);
                if (_isMatch(match) && match.similarity > bestSim) {
                    bestSim = match.similarity;
                    bestIdx = i;
                }
            }

            if (bestIdx >= 0) {
                _detectedExercise = _profile->exercises[bestIdx].type;
                _p1MatchCount++;
                _lastActiveTime = now;
                if (_p1MatchCount >= AUTO_TRANSITION_DEBOUNCE) {
                    _sequenceStep = 1;
                    _trackingStartTime = now;
                    _repStartTime = _lastP0MatchTime;      // rep began when user left P0
                    _confidence = bestSim * 100.0f;
                    _p0ReturnCount = 0;
                    DBG_AUTO("[DETECTED EXERCISE] CONFIRMED: %s (P1 sim=%.2f)\n",
                             exerciseTypeName(_detectedExercise), bestSim);
                    _transitionTo(AUTO_TRACKING);
                }
            } else {
                if (_p1MatchCount > 0) _p1MatchCount--;   // soft decay
            }
            break;
        }

        // ==================================================================
        // TRACKING: Confirmed exercise Ã¢â‚¬â€ track P0 -> P1 -> P0, count reps
        // ==================================================================
        case AUTO_TRACKING: {
            int exIdx = exerciseIndex(_detectedExercise);
            if (exIdx < 0 || exIdx >= MAX_EXERCISES) {
                reset();
                break;
            }
            const ExerciseProfile& ex = _profile->exercises[exIdx];

            if (_sequenceStep == 0) {
                // ---- At P0, waiting for the user to reach P1 ----
                PositionMatchResult p0 = _matchPosition(features, ex.positions[0]);
                if (_isMatch(p0)) {
                    _lastP0MatchTime = now;               // still at start position
                }
                // Resting at P0 for too long ends the set
                if (now - _lastActiveTime > AUTO_TRACKING_TIMEOUT_MS) {
                    DBG_AUTO("[STATE] TRACKING idle too long -> IDLE\n");
                    reset();
                    break;
                }

                PositionMatchResult match = _matchPosition(features, ex.positions[1]);
                if (_isMatch(match)) {
                    _p1MatchCount++;
                    _lastActiveTime = now;
                    _confidence = match.similarity * 100.0f;
                    if (_p1MatchCount >= AUTO_TRANSITION_DEBOUNCE) {
                        _sequenceStep = 1;
                        _p0ReturnCount = 0;
                        _repStartTime = _lastP0MatchTime; // rep began when user left P0
                        DBG_AUTO("[STATE] P0->P1 reached (sim=%.2f)\n", match.similarity);
                    }
                } else {
                    if (_p1MatchCount > 0) _p1MatchCount--;   // soft decay
                }

            } else {
                // ---- At P1, waiting for the user to return to P0 ----
                if (now - _repStartTime > AUTO_TRACKING_TIMEOUT_MS) {
                    DBG_AUTO("[STATE] Rep never completed (stuck at P1) -> IDLE, not counted\n");
                    reset();
                    break;
                }

                PositionMatchResult match = _matchPosition(features, ex.positions[0]);
                if (_isMatch(match)) {
                    _p0ReturnCount++;
                    _lastActiveTime = now;
                    _confidence = match.similarity * 100.0f;
                    if (_p0ReturnCount >= AUTO_TRANSITION_DEBOUNCE) {
                        // FULL SEQUENCE COMPLETE: P0 -> P1 -> P0 = 1 rep
                        unsigned long repDuration = now - _repStartTime;

                        // Calibrated timing window, widened for natural variation
                        float minAllowed = ex.minRepDurationMs * REP_TIMING_MIN_FACTOR;
                        float maxAllowed = ex.maxRepDurationMs * REP_TIMING_MAX_FACTOR;
                        if (minAllowed < REP_TIMING_ABS_MIN_MS) minAllowed = REP_TIMING_ABS_MIN_MS;

                        if (repDuration >= (unsigned long)minAllowed &&
                            repDuration <= (unsigned long)maxAllowed) {
                            _repCounts[exIdx]++;
                            DBG_AUTO("[REPETITION COUNT] %s count=%u duration=%lu ms  *** REP COUNTED ***\n",
                                     exerciseTypeName(_detectedExercise),
                                     (unsigned int)_repCounts[exIdx], repDuration);
                            _cooldownStartTime = now;
                            _transitionTo(AUTO_COOLDOWN);
                        } else {
                            DBG_AUTO("[STATE] REP REJECTED duration=%lu ms (allowed %.0f-%.0f)\n",
                                     repDuration, minAllowed, maxAllowed);
                            // Stay on this exercise, wait for the next full sequence
                            _sequenceStep = 0;
                            _p1MatchCount = 0;
                            _lastP0MatchTime = now;
                        }
                    }
                } else {
                    if (_p0ReturnCount > 0) _p0ReturnCount--;   // soft decay
                }
            }
            break;
        }

        // ==================================================================
        // COOLDOWN: Brief pause after a rep to prevent double counting
        // ==================================================================
        case AUTO_COOLDOWN: {
            if (now - _cooldownStartTime > AUTO_COOLDOWN_MS) {
                _sequenceStep = 0;
                _p1MatchCount = 0;
                _p0ReturnCount = 0;
                _lastP0MatchTime = now;
                _lastActiveTime = now;
                _transitionTo(AUTO_TRACKING);
            }
            break;
        }
    }
}
