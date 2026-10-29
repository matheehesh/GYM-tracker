#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"
#include "Types.h"

class DisplayManager {
public:
    DisplayManager();
    bool begin();   // Initialize SSD1306 display
    
    // Screen drawing functions
    void showSplash();  // Boot splash screen
    void showMenu(MenuItem selected);  // Main menu with cursor
    void showCalMenu(CalMenuItem selected);  // Calibration sub-menu
    void showAutoDetect(const char* exerciseName, uint32_t reps, float confidence, AutoDetectState state);
    void showExercise(ExerciseType type, uint32_t count, const char* stateName, float angle);
    void showCalibration(ExerciseType type, CalibrationPhase phase, const char* instruction, int progress, int reps);
    void showResults(const uint32_t counts[MAX_EXERCISES]);
    void showMessage(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr);
    void showCountdown(int seconds);
    void showAxisDebug(float ax, float ay, float az, float pitch, float roll);  // For axis mapping verification
    
    void clear();
    bool needsRefresh(unsigned long now); // Check if DISPLAY_REFRESH_MS has elapsed
    void markRefreshed();                 // Record last refresh time
    
private:
    unsigned long _lastRefreshTime;
    Adafruit_SSD1306 _display;
    
    // Helper to map enum values to strings
    const char* getMenuItemName(MenuItem item);
    const char* getCalMenuItemName(CalMenuItem item);
    const char* getExerciseName(ExerciseType type);
};

#endif // DISPLAY_MANAGER_H
