#include "mpu6050.h"
#include <string.h>
static mpu_result_t translate(bus_result_t result)
{
  switch (result) {
    case BUS_NACK: return MPU_NACK;
    case BUS_TIMEOUT: return MPU_TIMEOUT;
    default: return MPU_IO_ERROR;
  }
}
mpu_result_t mpu_begin(mpu6050_t *device, uint8_t *identity)
{
  uint8_t id = 0;
  bus_result_t status = device->bus.read(device->bus.context, device->address, 0x75, &id, 1);
  if (status != BUS_OK) return translate(status);
  *identity = id;
  if (id != 0x68) return MPU_BAD_ID;
  /* Wake on PLL; 44 Hz DLPF; 1 kHz/(9+1)=100 Hz; +/-2 g; gyro standby. */
  const uint8_t settings[][2] = {{0x6b, 1}, {0x1a, 3}, {0x19, 9}, {0x1c, 0}, {0x6c, 7}};
  for (size_t i = 0; i < sizeof(settings)/sizeof(settings[0]); ++i) {
    status = device->bus.write(device->bus.context, device->address, settings[i][0], &settings[i][1], 1);
    if (status != BUS_OK) return translate(status);
  }
  return MPU_OK;
}
mpu_result_t mpu_read(mpu6050_t *device, acceleration_t *sample)
{
  uint8_t bytes[6];
  bus_result_t status = device->bus.read(device->bus.context, device->address, 0x3b, bytes, sizeof(bytes));
  if (status != BUS_OK) return translate(status);
  acceleration_t next = {0};
  memcpy(next.raw, bytes, sizeof(bytes));
  for (size_t axis = 0; axis < 3; ++axis) {
    uint16_t word = (uint16_t)((uint16_t)bytes[2*axis] << 8) | bytes[2*axis+1];
    int32_t signed_value = (word & 0x8000U) ? (int32_t)word - 65536 : (int32_t)word;
    next.mg[axis] = signed_value * 1000 / 16384;
  }
  *sample = next;
  return MPU_OK;
}
const char *mpu_result_name(mpu_result_t result)
{
  switch (result) {
    case MPU_OK: return "OK";
    case MPU_NACK: return "NACK";
    case MPU_TIMEOUT: return "TIMEOUT";
    case MPU_BAD_ID: return "BAD_ID";
    default: return "IO_ERROR";
  }
}
