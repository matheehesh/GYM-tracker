#include "FeatureExtractor.h"
#include <math.h>

FeatureExtractor::FeatureExtractor() {
    reset();
}

void FeatureExtractor::begin() {
    reset();
}

void FeatureExtractor::reset() {
    bufferIdx = 0;
    bufferFull = false;
    for (int i = 0; i < FEATURE_WINDOW_SIZE; i++) {
        accelMagBuffer[i] = 0.0f;
    }
    lastPitch = 0.0f;
    lastRoll = 0.0f;
    lastAccelMag = 0.0f;
    wasRising = false;
    lastExtremum = 0.0f;
    lastPeakTimeLocal = 0;
    lastValleyTimeLocal = 0;
    lastPeakValueLocal = 0.0f;
    lastValleyValueLocal = 0.0f;
}

SensorFeatures FeatureExtractor::update(const FilteredData& filtered, float dt) {
    SensorFeatures f;
    f.timestamp = filtered.timestamp;

    f.accelMagnitude = sqrt(filtered.ax * filtered.ax + filtered.ay * filtered.ay + filtered.az * filtered.az);
    f.gyroMagnitude = sqrt(filtered.gx * filtered.gx + filtered.gy * filtered.gy + filtered.gz * filtered.gz);
    f.verticalAccel = filtered.linAy;
    
    f.pitch = filtered.pitch;
    f.roll = filtered.roll;
    
    if (dt > 0.0f) {
        f.pitchRate = (filtered.pitch - lastPitch) / dt;
        f.rollRate = (filtered.roll - lastRoll) / dt;
    } else {
        f.pitchRate = 0.0f;
        f.rollRate = 0.0f;
    }
    lastPitch = filtered.pitch;
    lastRoll = filtered.roll;

    // Update buffer
    accelMagBuffer[bufferIdx] = f.accelMagnitude;
    bufferIdx++;
    if (bufferIdx >= FEATURE_WINDOW_SIZE) {
        bufferIdx = 0;
        bufferFull = true;
    }

    // Statistics
    int count = bufferFull ? FEATURE_WINDOW_SIZE : bufferIdx;
    f.accelMagMean = 0.0f;
    f.accelMagPeak = 0.0f;
    f.accelMagVariance = 0.0f;
    f.gyroMagMean = f.gyroMagnitude;  // Simple: use instantaneous as single-sample mean
    f.gyroMagPeak = f.gyroMagnitude;  // Will be updated below if buffer has data
    f.movementEnergy = 0.0f;
    
    if (count > 0) {
        float sum = 0.0f;
        float maxVal = -9999.0f;
        for (int i = 0; i < count; i++) {
            sum += accelMagBuffer[i];
            if (accelMagBuffer[i] > maxVal) maxVal = accelMagBuffer[i];
        }
        f.accelMagMean = sum / count;
        f.accelMagPeak = maxVal;
        
        float varSum = 0.0f;
        for (int i = 0; i < count; i++) {
            float diff = accelMagBuffer[i] - f.accelMagMean;
            varSum += diff * diff;
        }
        f.accelMagVariance = varSum / count;
        f.movementEnergy = f.accelMagVariance;
    }

    // Peak detection with hysteresis
    bool isRising = f.accelMagnitude > lastAccelMag;
    f.isPeak = false;
    f.isValley = false;
    
    f.lastPeakValue = lastPeakValueLocal;
    f.lastValleyValue = lastValleyValueLocal;
    f.lastPeakTime = lastPeakTimeLocal;
    f.lastValleyTime = lastValleyTimeLocal;
    f.peakToValley = lastPeakValueLocal - lastValleyValueLocal;
    f.dominantPeriodMs = 0.0f;

    if (wasRising && !isRising) {
        // Potential peak
        if (f.accelMagnitude - lastExtremum > PEAK_MIN_PROMINENCE) {
            f.isPeak = true;
            lastPeakValueLocal = f.accelMagnitude;
            f.lastPeakValue = lastPeakValueLocal;
            
            if (lastPeakTimeLocal > 0) {
                f.dominantPeriodMs = f.timestamp - lastPeakTimeLocal;
            }
            lastPeakTimeLocal = f.timestamp;
            f.lastPeakTime = lastPeakTimeLocal;
            
            lastExtremum = f.accelMagnitude;
            f.peakToValley = lastPeakValueLocal - lastValleyValueLocal;
        }
    } else if (!wasRising && isRising) {
        // Potential valley
        if (lastExtremum - f.accelMagnitude > PEAK_MIN_PROMINENCE) {
            f.isValley = true;
            lastValleyValueLocal = f.accelMagnitude;
            f.lastValleyValue = lastValleyValueLocal;
            
            lastValleyTimeLocal = f.timestamp;
            f.lastValleyTime = lastValleyTimeLocal;
            
            lastExtremum = f.accelMagnitude;
            f.peakToValley = lastPeakValueLocal - lastValleyValueLocal;
        }
    }
    wasRising = isRising;
    lastAccelMag = f.accelMagnitude;
    
    // We compute peakCount and zeroCrossingCount
    int pCount = 0;
    int zCount = 0;
    if (count > 1) {
        bool lastSign = (accelMagBuffer[0] - f.accelMagMean) >= 0;
        for (int i = 1; i < count; i++) {
            bool sign = (accelMagBuffer[i] - f.accelMagMean) >= 0;
            if (sign != lastSign) {
                zCount++;
                lastSign = sign;
            }
            if (i > 0 && i < count - 1) {
                if (accelMagBuffer[i] > accelMagBuffer[i-1] && accelMagBuffer[i] > accelMagBuffer[i+1]) {
                    pCount++;
                }
            }
        }
    }
    f.peakCount = pCount;
    f.zeroCrossingCount = zCount;

    DBG_FEAT("Feat: mag=%.2f energy=%.2f peak=%d\n", f.accelMagnitude, f.movementEnergy, f.isPeak);
    return f;
}
