#ifndef AUTODETECTOR_H
#define AUTODETECTOR_H

#include "Types.h"
#include "Config.h"

class AutoDetector {
public:
    void begin(const UserProfileData* profile);
    void update(const SensorFeatures& features);
    void reset();
    void setProfile(const UserProfileData* profile);
    
    AutoDetectState getState() const;
    ExerciseType getDetectedExercise() const;
    int getCurrentSequenceStep() const;  // 0=at P0, 1=at P1
    float getConfidence() const;         // 0-100%
    uint32_t getRepCount(ExerciseType type) const;
    uint32_t getTotalReps() const;
    void resetCounts();

private:
    const UserProfileData* _profile;
    AutoDetectState _state;
    ExerciseType _detectedExercise;
    int _sequenceStep;           // 0=at P0, 1=at P1
    float _confidence;
    uint32_t _repCounts[MAX_EXERCISES];
    
    // Candidate tracking
    int _candidateCounts[MAX_EXERCISES]; // Consecutive P0 match counts per exercise
    int _p1MatchCount;                   // Consecutive P1 matches for transition debounce
    int _p0ReturnCount;                  // Consecutive P0 matches for rep completion
    
    // Timing
    unsigned long _candidateStartTime;
    unsigned long _trackingStartTime;
    unsigned long _repStartTime;
    unsigned long _cooldownStartTime;
    unsigned long _lastActiveTime;  // Last time any position matched
    unsigned long _lastDebugPrint;  // For periodic debug output
    unsigned long _lastP0MatchTime; // Last time live data matched P0 (rep start reference)
    
    // Internal methods
    PositionMatchResult _matchPosition(const SensorFeatures& features, const PositionProfile& pos) const;
    float _computeFeatureSimilarity(float value, float mean, float stdDev, float minTolerance) const;
    static bool _isMatch(const PositionMatchResult& r);
    void _transitionTo(AutoDetectState newState);
    void _printDebug(const SensorFeatures& features);
};

#endif // AUTODETECTOR_H
