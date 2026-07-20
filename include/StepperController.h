#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <TMCStepper.h>

#include "Config.h"

class StepperController
{
public:
  StepperController();

  void begin();
  void disableAllMotors();
  void disableMotor(Config::MotorId motor);

  long moveSteps(
    Config::MotorId motor,
    long signedSteps,
    bool positiveDirectionLevel,
    const Config::MotionProfile &profile,
    bool keepEnabledAfterMove = false
  );

  void printConnectionTests(Stream &output);

private:
  TMC5160Stepper leadDriver;
  TMC5160Stepper barrelDriver;
  TMC5160Stepper yawDriver;

  void configureDriver(
    TMC5160Stepper &driver,
    uint16_t currentMilliamps,
    uint16_t microsteps
  );

  void enableMotor(Config::MotorId motor);

  uint8_t getStepPin(Config::MotorId motor) const;
  uint8_t getDirectionPin(Config::MotorId motor) const;
  uint8_t getEnablePin(Config::MotorId motor) const;

  uint16_t calculatePulseDelay(
    unsigned long stepNumber,
    unsigned long totalSteps,
    const Config::MotionProfile &profile
  ) const;

  bool emergencyStopRequested();
};
