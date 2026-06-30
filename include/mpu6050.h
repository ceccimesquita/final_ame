#pragma once

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include <stdint.h>
#include <math.h>

#define MPU6050_ADDR          0x68
#define MPU6050_SMPLRT_DIV    0x19
#define MPU6050_CONFIG_REG    0x1A
#define MPU6050_GYRO_CONFIG   0x1B
#define MPU6050_ACCEL_CONFIG  0x1C
#define MPU6050_PWR_MGMT_1    0x6B
#define MPU6050_WHO_AM_I      0x75
#define MPU6050_ACCEL_XOUT_H  0x3B

#define GYRO_LSB   131.0f
#define ACCEL_LSB  16384.0f
#define TEMP_DIV   340.0f
#define TEMP_OFS   36.53f

#define FILTER_COEF      0.98f
#define CALIB_NB_MES     3000
#define RAD_TO_DEG       57.295779513f

class MPU6050_light {
public:
    MPU6050_light(i2c_inst_t *i2c, uint sda, uint scl,
                  uint freq = 400000, uint8_t addr = MPU6050_ADDR);

    int  begin();
    void calcOffsets(bool isGyro = true, bool isAccel = true);
    bool update();

    float getTemp()    const { return temp; }
    float getAccX()    const { return accX; }
    float getAccY()    const { return accY; }
    float getAccZ()    const { return accZ; }
    float getGyroX()   const { return gyroX; }
    float getGyroY()   const { return gyroY; }
    float getGyroZ()   const { return gyroZ; }
    float getAngleX()  const { return angleX; }
    float getAngleY()  const { return angleY; }
    float getAngleZ()  const { return angleZ; }

    void setFilterGyroCoef(float c) { filterCoef = c; }
    void setGyroOffsets(float x, float y, float z) { gyroOffX=x; gyroOffY=y; gyroOffZ=z; }
    void setAccOffsets(float x, float y, float z)  { accOffX=x;  accOffY=y;  accOffZ=z;  }

    float accOffX=0,  accOffY=0,  accOffZ=0;
    float gyroOffX=0, gyroOffY=0, gyroOffZ=0;

private:
    i2c_inst_t *_i2c;
    uint8_t     _addr;
    float       filterCoef = FILTER_COEF;

    float accX=0,   accY=0,   accZ=0;
    float gyroX=0,  gyroY=0,  gyroZ=0;
    float angleX=0, angleY=0, angleZ=0;
    float temp=0;
    uint32_t prevMs=0;
    bool     _lastReadOk = false;

    void    writeReg(uint8_t reg, uint8_t val);
    uint8_t readReg(uint8_t reg);
    bool    readAll();
};
