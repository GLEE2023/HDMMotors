#pragma once

#include <Arduino.h>

// Records attach/write calls in the hardware log (see hw_sim.h).
class Servo {
public:
  uint8_t attach(int pin);
  void write(int angle);

private:
  int attachedPin = -1;
};
