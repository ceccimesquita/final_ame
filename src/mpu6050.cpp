#include "mpu6050.h"
#include <stdio.h>

MPU6050_light::MPU6050_light(i2c_inst_t *i2c, uint sda, uint scl, uint freq, uint8_t addr)
    : _i2c(i2c), _addr(addr)
{
    i2c_init(_i2c, freq);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);
}

int MPU6050_light::begin() {
    uint8_t who = readReg(MPU6050_WHO_AM_I);
    if (who == 0xFF) {
        printf("WHO_AM_I: sem resposta (0xFF) - verifique as conexoes\n");
        return -1;
    }
    if (who != 0x68 && who != 0x72 && who != 0x70 && who != 0x71) {
        printf("WHO_AM_I desconhecido: 0x%02X - tentando continuar...\n", who);
    }

    writeReg(MPU6050_PWR_MGMT_1,   0x01);
    sleep_ms(100);
    writeReg(MPU6050_SMPLRT_DIV,   0x00);
    writeReg(MPU6050_CONFIG_REG,   0x00);
    writeReg(MPU6050_GYRO_CONFIG,  0x00);
    writeReg(MPU6050_ACCEL_CONFIG, 0x00);

    prevMs = to_ms_since_boot(get_absolute_time());
    return 0;
}

bool MPU6050_light::readAll() {
    uint8_t buf[14];
    uint8_t reg = MPU6050_ACCEL_XOUT_H;

    int wr = i2c_write_timeout_us(_i2c, _addr, &reg, 1, true,  10000);
    if (wr < 0) { _lastReadOk = false; return false; }

    int rd = i2c_read_timeout_us(_i2c, _addr, buf, 14, false, 10000);
    if (rd != 14) { _lastReadOk = false; return false; }

    int16_t rAX = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t rAY = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t rAZ = (int16_t)((buf[4]  << 8) | buf[5]);
    int16_t rT  = (int16_t)((buf[6]  << 8) | buf[7]);
    int16_t rGX = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t rGY = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t rGZ = (int16_t)((buf[12] << 8) | buf[13]);

    accX  = (rAX / ACCEL_LSB) - accOffX;
    accY  = (rAY / ACCEL_LSB) - accOffY;
    accZ  = (rAZ / ACCEL_LSB) - accOffZ;
    gyroX = (rGX / GYRO_LSB)  - gyroOffX;
    gyroY = (rGY / GYRO_LSB)  - gyroOffY;
    gyroZ = (rGZ / GYRO_LSB)  - gyroOffZ;
    temp  = (rT  / TEMP_DIV)  + TEMP_OFS;

    _lastReadOk = true;
    return true;
}

bool MPU6050_light::update() {
    bool ok = readAll();
    if (!ok) return false;

    uint32_t now = to_ms_since_boot(get_absolute_time());
    float dt = (now - prevMs) * 0.001f;
    prevMs = now;

    float accAngleX =  atan2f(accY, accZ) * RAD_TO_DEG;
    float accAngleY = -atan2f(accX, accZ) * RAD_TO_DEG;

    angleX = filterCoef * (angleX + gyroX * dt) + (1.0f - filterCoef) * accAngleX;
    angleY = filterCoef * (angleY + gyroY * dt) + (1.0f - filterCoef) * accAngleY;
    angleZ += gyroZ * dt;
    return true;
}

void MPU6050_light::calcOffsets(bool isGyro, bool isAccel) {
    if (isGyro) gyroOffX = gyroOffY = gyroOffZ = 0;
    if (isAccel) accOffX  = accOffY  = accOffZ  = 0;

    double sgX=0, sgY=0, sgZ=0, saX=0, saY=0, saZ=0;

    for (int i = 0; i < CALIB_NB_MES; i++) {
        readAll();
        if (isGyro)  { sgX += gyroX; sgY += gyroY; sgZ += gyroZ; }
        if (isAccel) { saX += accX;  saY += accY;  saZ += accZ;  }
        sleep_ms(1);
    }

    if (isGyro) {
        gyroOffX = (float)(sgX / CALIB_NB_MES);
        gyroOffY = (float)(sgY / CALIB_NB_MES);
        gyroOffZ = (float)(sgZ / CALIB_NB_MES);
    }
    if (isAccel) {
        accOffX = (float)(saX / CALIB_NB_MES);
        accOffY = (float)(saY / CALIB_NB_MES);
        accOffZ = (float)(saZ / CALIB_NB_MES) - 1.0f;
    }
}

void MPU6050_light::writeReg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_timeout_us(_i2c, _addr, buf, 2, false, 10000);
}

uint8_t MPU6050_light::readReg(uint8_t reg) {
    uint8_t val = 0xFF;
    int r = i2c_write_timeout_us(_i2c, _addr, &reg, 1, true, 10000);
    if (r < 0) return 0xFF;
    i2c_read_timeout_us(_i2c, _addr, &val, 1, false, 10000);
    return val;
}
