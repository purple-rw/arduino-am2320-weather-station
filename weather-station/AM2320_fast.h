#ifndef AM2320_FAST_H
#define AM2320_FAST_H

#define AM2320_FAST_DEVICE_ADDRESS  0x5C // (0xB8 >> 1)

#define AM2320_FAST_SUCCESS                       0
#define AM2320_FAST_ERROR_FUNCTION_CODE_MISMATCH  1
#define AM2320_FAST_ERROR_DATA_SIZE_MISMATCH      2
#define AM2320_FAST_ERROR_CRC_MISMATCH            3

#include <Arduino.h>
#include <Wire.h>

class AM2320_fast {
  public:
    AM2320_fast();
    void setWire(TwoWire *wire);
    int update();
    int temperature;    // unit = 0.1C, -400 to +850
    int humidity;       // unit = 0.1%, 0 to 1000

  private:
    TwoWire* myWire;
    uint16_t crc16(uint8_t *ptr, uint8_t len);
};

#endif
