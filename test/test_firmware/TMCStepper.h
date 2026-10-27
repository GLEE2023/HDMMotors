#pragma once

#include <Arduino.h>

// Fake TMC5160 driver. Settings land in hw::tmc (hw_sim.h) because the real
// driver object is private inside StepperController.
class TMC5160Stepper {
public:
  TMC5160Stepper(uint16_t chipSelectPin, float rSense);

  void begin();
  void toff(uint8_t value);
  void rms_current(uint16_t milliamps);
  void microsteps(uint16_t value);
  void en_pwm_mode(bool enabled);
  uint8_t test_connection();

private:
  uint16_t chipSelect;
  float senseResistor;
};
