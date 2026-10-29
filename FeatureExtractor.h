#ifndef FEATUREEXTRACTOR_H
#define FEATUREEXTRACTOR_H

#include "Config.h"
#include "Types.h"

class FeatureExtractor {
public:
    FeatureExtractor();
    void begin();
    SensorFeatures update(const FilteredData& filtered, float dt);
    void reset();

private:
    float accelMagBuffer[FEATURE_WINDOW_SIZE];
    int bufferIdx;
    bool bufferFull;
    
    float lastPitch;
    float lastRoll;
    float lastAccelMag;
    
    // Peak detection
    bool wasRising;
    float lastExtremum;
    unsigned long lastPeakTimeLocal;
    unsigned long lastValleyTimeLocal;
    float lastPeakValueLocal;
    float lastValleyValueLocal;
};

#endif // FEATUREEXTRACTOR_H
