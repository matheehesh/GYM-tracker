#ifndef USER_PROFILE_H
#define USER_PROFILE_H

#include "Types.h"
#include "Config.h"

class UserProfile {
public:
    UserProfileData data;
    
    void begin();                              // Load from NVS or set defaults
    void setDefaults();                        // Clear all exercise profiles
    bool loadFromNVS();                        // Load saved data
    bool saveToNVS();                          // Save current data
    void resetAll();                           // Clear NVS and reset
    void resetExercise(ExerciseType type);      // Reset one exercise profile
    bool isCalibrated(ExerciseType type) const; // Check if specific exercise calibrated
    bool isAnyCalibrated() const;              // Any exercise calibrated?
    void printCalibration() const;             // Print all profiles to Serial
    void printExerciseProfile(ExerciseType type) const; // Print one exercise
};

#endif // USER_PROFILE_H
