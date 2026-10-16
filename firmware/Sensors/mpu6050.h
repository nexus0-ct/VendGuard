#ifndef VG_MPU6050_H
#define VG_MPU6050_H
#include "register_bus.h"
typedef enum { MPU_OK, MPU_NACK, MPU_TIMEOUT, MPU_IO_ERROR, MPU_BAD_ID } mpu_result_t;
typedef struct { register_bus_t bus; uint8_t address; } mpu6050_t;
typedef struct { int32_t mg[3]; uint8_t raw[6]; } acceleration_t;
mpu_result_t mpu_begin(mpu6050_t *device, uint8_t *identity);
mpu_result_t mpu_read(mpu6050_t *device, acceleration_t *sample);
const char *mpu_result_name(mpu_result_t result);
#endif
