#include "DisplayManager.h"
#include <stdio.h>

DisplayManager::DisplayManager() : _lastRefreshTime(0), _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN) {}

bool DisplayManager::begin() {
    if(!_display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        return false;
    }
    _display.clearDisplay();
    _display.setTextColor(SSD1306_WHITE);
    _display.display();
    return true;
}

void DisplayManager::clear() {
    _display.clearDisplay();
}

bool DisplayManager::needsRefresh(unsigned long now) {
    return (now - _lastRefreshTime >= DISPLAY_REFRESH_MS);
}

void DisplayManager::markRefreshed() {
    _lastRefreshTime = millis();
}

void DisplayManager::showSplash() {
    clear();
    _display.setTextSize(1);
    
    // Center text approximately
    _display.setCursor(30, 10);
    _display.println("GYM TRACKER");
    _display.setCursor(30, 20);
    _display.println("----------");
    _display.setCursor(36, 30);
    _display.println("ESP32-C3");
    _display.setCursor(48, 40);
    _display.println("v1.0");
    
    _display.setCursor(26, 55);
    _display.println("Tap to start");
    
    _display.display();
}

const char* DisplayManager::getMenuItemName(MenuItem item) {
    switch (item) {
        case MENU_RESUME: return "Resume";
        case MENU_CALIBRATE: return "Calibrate";
        case MENU_RESULTS: return "Results";
        case MENU_RESET: return "Reset All";
        default: return "";
    }
}

void DisplayManager::showMenu(MenuItem selected) {
    clear();
    _display.setTextSize(1);
    _display.setCursor(36, 0);
    _display.println("MAIN MENU");
    _display.setCursor(30, 8);
    _display.println("-----------");
    
    int startIndex = (int)selected - 2;
    if (startIndex < 0) startIndex = 0;
    if (startIndex > MENU_ITEM_COUNT - 5) startIndex = max(0, (int)MENU_ITEM_COUNT - 5);
    
    for (int i = 0; i < 5 && (startIndex + i) < MENU_ITEM_COUNT; i++) {
        int itemIndex = startIndex + i;
        _display.setCursor(0, 16 + i * 8);
        if (itemIndex == selected) {
            _display.print("> ");
        } else {
            _display.print("  ");
        }
        _display.println(getMenuItemName((MenuItem)itemIndex));
    }
    
    _display.setCursor(0, 56);
    _display.println("TAP=next  2TAP=sel");
    _display.display();
}

const char* DisplayManager::getCalMenuItemName(CalMenuItem item) {
    switch (item) {
        case CAL_MENU_WALKING: return "Walking";
        case CAL_MENU_SQUAT: return "Squat";
        case CAL_MENU_JJ: return "Jumping Jack";
        case CAL_MENU_SITUP: return "Sit-up";
        case CAL_MENU_ALL: return "All";
        case CAL_MENU_BACK: return "Back";
        default: return "";
    }
}

void DisplayManager::showCalMenu(CalMenuItem selected) {
    clear();
    _display.setTextSize(1);
    _display.setCursor(38, 0);
    _display.println("CAL MENU");
    _display.setCursor(30, 8);
    _display.println("----------");
    
    int startIndex = (int)selected - 2;
    if (startIndex < 0) startIndex = 0;
    if (startIndex > CAL_MENU_ITEM_COUNT - 5) startIndex = max(0, (int)CAL_MENU_ITEM_COUNT - 5);
    
    for (int i = 0; i < 5 && (startIndex + i) < CAL_MENU_ITEM_COUNT; i++) {
        int itemIndex = startIndex + i;
        _display.setCursor(0, 16 + i * 8);
        if (itemIndex == selected) {
            _display.print("> ");
        } else {
            _display.print("  ");
        }
        _display.println(getCalMenuItemName((CalMenuItem)itemIndex));
    }
    
    _display.setCursor(0, 56);
    _display.println("TAP=next  2TAP=sel");
    _display.display();
}

const char* DisplayManager::getExerciseName(ExerciseType type) {
    switch (type) {
        case EXERCISE_WALKING: return "WALKING";
        case EXERCISE_SQUAT: return "SQUAT";
        case EXERCISE_JUMPING_JACK: return "JUMP JACK";
        case EXERCISE_SIT_UP: return "SIT-UP";
        default: return "UNKNOWN";
    }
}

void DisplayManager::showExercise(ExerciseType type, uint32_t count, const char* stateName, float angle) {
    clear();
    _display.setTextSize(1);
    
    const char* title = getExerciseName(type);
    int16_t x1, y1;
    uint16_t w, h;
    _display.getTextBounds(title, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor((SCREEN_WIDTH - w) / 2, 0);
    _display.println(title);
    
    _display.setCursor((SCREEN_WIDTH - 60) / 2, 8);
    _display.println("----------");
    
    _display.setCursor(0, 20);
    _display.print("Count: ");
    _display.setTextSize(2);
    _display.println(count);
    
    _display.setTextSize(1);
    _display.setCursor(0, 40);
    _display.print("State: ");
    _display.println(stateName ? stateName : "UNKNOWN");
    
    _display.setCursor(0, 48);
    _display.print("Angle: ");
    _display.print(angle, 1);
    _display.println((char)247); // Degree symbol
    
    _display.setCursor(0, 56);
    _display.println("SHAKE=stop");
    _display.display();
}

void DisplayManager::showCalibration(ExerciseType type, CalibrationPhase phase, const char* instruction, int progress, int reps) {
    clear();
    _display.setTextSize(1);
    
    _display.setCursor(0, 0);
    _display.print("CAL: ");
    _display.println(getExerciseName(type));
    _display.println("----------");
    
    _display.setCursor(0, 16);
    _display.println(instruction ? instruction : "");
    
    _display.setCursor(0, 32);
    _display.print("Reps: ");
    _display.print(reps);
    
    _display.setCursor(0, 40);
    _display.print("Progress: [");
    int bars = progress / 10;
    for (int i = 0; i < 10; i++) {
        if (i < bars) _display.print("=");
        else _display.print(" ");
    }
    _display.println("]");
    
    _display.setCursor(0, 56);
    _display.println("TAP=next");
    _display.display();
}

void DisplayManager::showResults(const uint32_t counts[MAX_EXERCISES]) {
    clear();
    _display.setTextSize(1);
    
    _display.setCursor(40, 0);
    _display.println("RESULTS");
    _display.setCursor(34, 8);
    _display.println("---------");
    
    _display.setCursor(0, 16);
    _display.printf("Walk:  %u\n", (unsigned int)counts[0]);
    _display.setCursor(0, 24);
    _display.printf("Squat: %u\n", (unsigned int)counts[1]);
    _display.setCursor(0, 32);
    _display.printf("JJ:    %u\n", (unsigned int)counts[2]);
    _display.setCursor(0, 40);
    _display.printf("Sit-up:  %u\n", (unsigned int)counts[3]);
    
    _display.setCursor(0, 56);
    _display.println("PRESS=back");
    _display.display();
}

void DisplayManager::showMessage(const char* line1, const char* line2, const char* line3) {
    clear();
    _display.setTextSize(1);
    if (line1) {
        _display.setCursor(0, 16);
        _display.println(line1);
    }
    if (line2) {
        _display.setCursor(0, 26);
        _display.println(line2);
    }
    if (line3) {
        _display.setCursor(0, 36);
        _display.println(line3);
    }
    _display.display();
}

void DisplayManager::showCountdown(int seconds) {
    clear();
    _display.setTextSize(2);
    _display.setCursor(56, 24);
    _display.println(seconds);
    _display.display();
}

void DisplayManager::showAxisDebug(float ax, float ay, float az, float pitch, float roll) {
    clear();
    _display.setTextSize(1);
    _display.setCursor(30, 0);
    _display.println("AXIS DEBUG");
    
    _display.setCursor(0, 16);
    _display.printf("ax=%.2f ay=%.2f\n", ax, ay);
    
    _display.setCursor(0, 26);
    _display.printf("az=%.2f\n", az);
    
    _display.setCursor(0, 46);
    _display.printf("P=%.1f R=%.1f\n", pitch, roll);
    _display.display();
}

void DisplayManager::showAutoDetect(const char* exerciseName, uint32_t reps, float confidence, AutoDetectState state) {
    clear();
    _display.setTextSize(1);
    
    _display.setCursor(30, 0);
    _display.println("AUTO DETECT");
    _display.setCursor(30, 8);
    _display.println("----------");
    
    if (state == AUTO_IDLE) {
        _display.setCursor(0, 20);
        _display.println("Waiting...");
        _display.setCursor(0, 36);
        _display.println("Move to start");
        _display.setCursor(0, 56);
        _display.println("PRESS=menu");
    } else if (state == AUTO_CANDIDATE) {
        _display.setCursor(0, 20);
        _display.println("Detecting...");
        _display.setCursor(0, 30);
        if (exerciseName) _display.println(exerciseName);
        
        _display.setCursor(0, 46);
        _display.print("Conf: ");
        _display.print((int)confidence);
        _display.println("%");
        
        _display.setCursor(0, 56);
        _display.println("PRESS=menu");
    } else { // AUTO_TRACKING or AUTO_COOLDOWN
        _display.setCursor(0, 20);
        if (exerciseName) {
            String name = String(exerciseName);
            name.toUpperCase();
            _display.println(name.c_str());
        }
        
        _display.setCursor(0, 34);
        _display.print("Reps: ");
        _display.setTextSize(2);
        _display.println(reps);
        
        _display.setTextSize(1);
        _display.setCursor(0, 46);
        _display.print("Conf: ");
        _display.print((int)confidence);
        _display.println("%");
        
        _display.setCursor(0, 56);
        _display.println("PRESS=menu");
    }
    
    _display.display();
}
