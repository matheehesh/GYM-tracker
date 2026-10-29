#ifndef SENSORMANAGER_H
#define SENSORMANAGER_H

#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "Config.h"
#include "Types.h"

class SensorManager {
public:
    SensorManager();
    bool begin();
    bool read(RawSensorData& raw);
    MappedSensorData mapAxes(const RawSensorData& raw);
    void calibrateOffsets();

private:
    Adafruit_MPU6050 mpu;
    SensorAxisConfig axisConfig;
    
    // Offsets
    float ax_off, ay_off, az_off;
    float gx_off, gy_off, gz_off;
};

#endif // SENSORMANAGER_H
