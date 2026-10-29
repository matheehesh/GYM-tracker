#include "SensorManager.h"
#include <Arduino.h>
#include <math.h>

SensorManager::SensorManager() : ax_off(0), ay_off(0), az_off(0), gx_off(0), gy_off(0), gz_off(0) {
    axisConfig.side = (SensorSide)SENSOR_SIDE;
    axisConfig.xAxis = AXIS_MAP_WORLD_X;
    axisConfig.yAxis = AXIS_MAP_WORLD_Y;
    axisConfig.zAxis = AXIS_MAP_WORLD_Z;
    axisConfig.xSign = AXIS_SIGN_WORLD_X;
    axisConfig.ySign = AXIS_SIGN_WORLD_Y;
    axisConfig.zSign = AXIS_SIGN_WORLD_Z;
}

bool SensorManager::begin() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
    if (!mpu.begin(MPU_I2C_ADDRESS, &Wire)) {
        DBG_PRINTLN("Failed to find MPU6050 chip");
        return false;
    }
    
    mpu.setAccelerometerRange((mpu6050_accel_range_t)MPU_ACCEL_RANGE);
    mpu.setGyroRange((mpu6050_gyro_range_t)MPU_GYRO_RANGE);
    mpu.setFilterBandwidth((mpu6050_bandwidth_t)MPU_FILTER_BW);

    DBG_PRINTLN("MPU6050 initialized OK");
    return true;
}

void SensorManager::calibrateOffsets() {
    DBG_PRINTLN("Calibrating offsets... Keep sensor still.");
    delay(SENSOR_OFFSET_SETTLE_MS);

    float sum_ax = 0, sum_ay = 0, sum_az = 0;
    float sum_gx = 0, sum_gy = 0, sum_gz = 0;

    for (int i = 0; i < SENSOR_OFFSET_SAMPLES; i++) {
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);
        sum_ax += a.acceleration.x;
        sum_ay += a.acceleration.y;
        sum_az += a.acceleration.z;
        sum_gx += g.gyro.x;
        sum_gy += g.gyro.y;
        sum_gz += g.gyro.z;
        delay(SENSOR_SAMPLE_INTERVAL_MS);
    }

    ax_off = sum_ax / SENSOR_OFFSET_SAMPLES;
    ay_off = sum_ay / SENSOR_OFFSET_SAMPLES;
    az_off = sum_az / SENSOR_OFFSET_SAMPLES;
    gx_off = sum_gx / SENSOR_OFFSET_SAMPLES;
    gy_off = sum_gy / SENSOR_OFFSET_SAMPLES;
    gz_off = sum_gz / SENSOR_OFFSET_SAMPLES;

    // We expect gravity (~9.81) on one axis, let's not zero it out.
    if (abs(ax_off) > 5.0f) ax_off -= (ax_off > 0 ? GRAVITY_MS2 : -GRAVITY_MS2);
    if (abs(ay_off) > 5.0f) ay_off -= (ay_off > 0 ? GRAVITY_MS2 : -GRAVITY_MS2);
    if (abs(az_off) > 5.0f) az_off -= (az_off > 0 ? GRAVITY_MS2 : -GRAVITY_MS2);
}

bool SensorManager::read(RawSensorData& raw) {
    sensors_event_t a, g, temp;
    if (!mpu.getEvent(&a, &g, &temp)) return false;
    
    raw.ax = a.acceleration.x - ax_off;
    raw.ay = a.acceleration.y - ay_off;
    raw.az = a.acceleration.z - az_off;
    
    // Adafruit returns rad/s, we need deg/s
    raw.gx = (g.gyro.x - gx_off) * 180.0f / PI;
    raw.gy = (g.gyro.y - gy_off) * 180.0f / PI;
    raw.gz = (g.gyro.z - gz_off) * 180.0f / PI;
    
    raw.timestamp = millis();
    
    DBG_RAW("Raw: ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f\n",
            raw.ax, raw.ay, raw.az, raw.gx, raw.gy, raw.gz);
            
    return true;
}

MappedSensorData SensorManager::mapAxes(const RawSensorData& raw) {
    MappedSensorData mapped;
    
    float rawAccel[3] = {raw.ax, raw.ay, raw.az};
    float rawGyro[3]  = {raw.gx, raw.gy, raw.gz};
    
    mapped.ax = rawAccel[axisConfig.xAxis] * axisConfig.xSign;
    mapped.ay = rawAccel[axisConfig.yAxis] * axisConfig.ySign;
    mapped.az = rawAccel[axisConfig.zAxis] * axisConfig.zSign;
    
    mapped.gx = rawGyro[axisConfig.xAxis] * axisConfig.xSign;
    mapped.gy = rawGyro[axisConfig.yAxis] * axisConfig.ySign;
    mapped.gz = rawGyro[axisConfig.zAxis] * axisConfig.zSign;
    
    mapped.timestamp = raw.timestamp;
    
    DBG_AXIS("Mapped: ax=%.2f ay=%.2f az=%.2f gx=%.2f gy=%.2f gz=%.2f\n",
             mapped.ax, mapped.ay, mapped.az, mapped.gx, mapped.gy, mapped.gz);
             
    return mapped;
}
