#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <TMCStepper.h>

#include "Config.h"

// Low-level driver wrapper for the three TMC5160 stepper motors.
class StepperController {
public:
  // Creates the controller object and binds it to the configured driver pins.
  StepperController();

  // Initializes pins, SPI, and the configured stepper drivers.
  void begin();
  // Disables every motor output stage at once.
  void disableAllMotors();
  // Disables the selected motor while leaving the others alone.
  void disableMotor(Config::MotorId motor);

  // Sends a signed step pulse train for one motor using the requested motion profile.
  long moveSteps(
    Config::MotorId motor,
    long signedSteps,
    bool positiveDirectionLevel,
    const Config::MotionProfile &profile,
    bool keepEnabledAfterMove = false
  );

  // Tests the SPI connection to each installed TMC5160 driver.
  void printConnectionTests(Stream &output);

private:
  TMC5160Stepper leadDriver;
  TMC5160Stepper barrelDriver;
  TMC5160Stepper yawDriver;

  // Applies the standard current and microstep settings for one TMC5160 driver.
  void configureDriver(
    TMC5160Stepper &driver,
    uint16_t currentMilliamps,
    uint16_t microsteps
  );

  // Enables one motor and waits briefly for the driver to settle.
  void enableMotor(Config::MotorId motor);

  // Maps a logical motor to the pin that toggles its step signal.
  uint8_t getStepPin(Config::MotorId motor) const;
  // Maps a logical motor to the pin that selects its movement direction.
  uint8_t getDirectionPin(Config::MotorId motor) const;
  // Maps a logical motor to the pin that enables or disables its output stage.
  uint8_t getEnablePin(Config::MotorId motor) const;

  // Calculates the delay between step pulses so the move accelerates and decelerates smoothly.
  uint16_t calculatePulseDelay(
    unsigned long stepNumber,
    unsigned long totalSteps,
    const Config::MotionProfile &profile
  ) const;

  // Checks whether the serial input contains an emergency-stop request.
  bool emergencyStopRequested();
};
