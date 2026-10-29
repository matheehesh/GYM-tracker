#include "SignalFilter.h"
#include <math.h>

SignalFilter::SignalFilter() {
    reset();
}

void SignalFilter::begin() {
    reset();
}

void SignalFilter::reset() {
    state.ax = state.ay = state.az = 0;
    state.gx = state.gy = state.gz = 0;
    state.pitch = state.roll = 0;
    state.linAx = state.linAy = state.linAz = 0;
    state.gravX = state.gravY = state.gravZ = 0;
    state.timestamp = 0;
}

FilteredData SignalFilter::update(const MappedSensorData& mapped, float dt) {
    // Low pass filter for accel
    state.ax = LPF_ACCEL_ALPHA * mapped.ax + (1.0f - LPF_ACCEL_ALPHA) * state.ax;
    state.ay = LPF_ACCEL_ALPHA * mapped.ay + (1.0f - LPF_ACCEL_ALPHA) * state.ay;
    state.az = LPF_ACCEL_ALPHA * mapped.az + (1.0f - LPF_ACCEL_ALPHA) * state.az;

    // Low pass filter for gyro
    state.gx = LPF_GYRO_ALPHA * mapped.gx + (1.0f - LPF_GYRO_ALPHA) * state.gx;
    state.gy = LPF_GYRO_ALPHA * mapped.gy + (1.0f - LPF_GYRO_ALPHA) * state.gy;
    state.gz = LPF_GYRO_ALPHA * mapped.gz + (1.0f - LPF_GYRO_ALPHA) * state.gz;

    // Pitch and Roll from Accelerometer (World frame Y=UP)
    // Pitch (forward lean) = angle of X-axis from horizontal
    float accelPitch = atan2(state.ax, sqrt(state.ay * state.ay + state.az * state.az)) * 180.0f / PI;
    // Roll (side lean)
    float accelRoll = atan2(-state.az, state.ay) * 180.0f / PI;

    // Complementary Filter
    // Note: If pitch moves opposite to expected direction when leaning forward, negate gyroZ
    state.pitch = COMP_FILTER_ALPHA * (state.pitch + state.gz * dt) + (1.0f - COMP_FILTER_ALPHA) * accelPitch;
    state.roll = COMP_FILTER_ALPHA * (state.roll + state.gx * dt) + (1.0f - COMP_FILTER_ALPHA) * accelRoll;

    // Gravity estimation (simple low-pass)
    state.gravX = (1.0f - HPF_ALPHA) * state.ax + HPF_ALPHA * state.gravX;
    state.gravY = (1.0f - HPF_ALPHA) * state.ay + HPF_ALPHA * state.gravY;
    state.gravZ = (1.0f - HPF_ALPHA) * state.az + HPF_ALPHA * state.gravZ;

    // Linear acceleration
    state.linAx = state.ax - state.gravX;
    state.linAy = state.ay - state.gravY;
    state.linAz = state.az - state.gravZ;

    state.timestamp = mapped.timestamp;

    DBG_FILT("Filt: p=%.2f r=%.2f linAy=%.2f\n", state.pitch, state.roll, state.linAy);
    
    return state;
}
