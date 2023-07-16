#include "AM2320_fast.h"

AM2320_fast::AM2320_fast() {
  myWire = &Wire;
}

void AM2320_fast::setWire(TwoWire* wire) {
  myWire = wire;
}

int AM2320_fast::update() {
  static const int buffLen = 8;
  uint8_t buf[buffLen];

  // Wake up sensor
  myWire->beginTransmission(AM2320_FAST_DEVICE_ADDRESS);
  myWire->endTransmission();
  delay(10);

  /*  Function code 0x03 = read register data
                    0x10 = write multiple registers
      Register addr 0x00 = Humidity MSB
                    0x01 = Humidity LSB
                    0x02 = Temperature MSB
                    0x03 = Temperature LSB
                    0x08 = Model MSB
                    0x09 = Model LSB
                    0x0A = Version number
                    0x0B = Device ID (bit 24-31)
                    0x0C = Device ID (bit 16-23)
                    0x0D = Device ID (bit 8-15)
                    0x0E = Device ID (bit 0-7)
                    0x0F = Status
   */
  myWire->beginTransmission(AM2320_FAST_DEVICE_ADDRESS);
  myWire->write(0x03);    // function code
  myWire->write(0x00);    // register addr
  myWire->write(0x04);    // bytes to read
  int result = myWire->endTransmission();
  if (result != AM2320_FAST_SUCCESS)
    return result;
  delay(2);    // delay 1.5ms or more

  // Receive data
  myWire->requestFrom(AM2320_FAST_DEVICE_ADDRESS, buffLen);
  for (uint8_t i=0; i<buffLen; i++) {
    buf[i] = myWire->read();
  }

  // Function Code check
  if (buf[0] != 0x03)
    return AM2320_FAST_ERROR_FUNCTION_CODE_MISMATCH;

  // Data size check
  if (buf[1] != 0x04)
    return AM2320_FAST_ERROR_DATA_SIZE_MISMATCH;

  // CRC check
  uint16_t crc = ((uint16_t) buf[7] << 8) | buf[6];
  if (crc != crc16(buf, 6))
    return AM2320_FAST_ERROR_CRC_MISMATCH;

  humidity = ((int) buf[2] << 8) | buf[3];
  temperature = ((int) buf[4] << 8) | buf[5];
  if (temperature & 0x8000)
    temperature = - (temperature & 0x7FFF);

  return AM2320_FAST_SUCCESS;
}

uint16_t AM2320_fast::crc16(uint8_t *ptr, uint8_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= *ptr++;
    for (uint8_t s=0; s<8; s++) {
      if (crc & 0x01) {
        crc >>= 1;
        crc ^= 0xA001;
      } else
        crc >>= 1;
    }
  }
  return crc;
}
