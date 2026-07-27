#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <TMCStepper.h>

#include "Config.h"

// Low-level driver wrapper for the motion system.
// The lead screw remains a TMC5160 over SPI, while barrel and yaw use
// standalone TMC2209 STEP/DIR drivers without UART.
class StepperController {
public:
  StepperController();

  // Initializes pins, SPI, and the configured lead-screw driver.
  void begin();
  // Stops generating pulses for every motor at once.
  void disableAllMotors();
  // Stops pulses for the selected motor while leaving the others alone.
  void disableMotor(Config::MotorId motor);

  // Sends a signed step pulse train for one motor using the requested motion profile.
  long moveSteps(
    Config::MotorId motor,
    long signedSteps,
    bool positiveDirectionLevel,
    const Config::MotionProfile &profile,
    bool keepEnabledAfterMove = false
  );

  // Tests the SPI connection to the installed TMC5160 driver.
  void printConnectionTests(Stream &output);

private:
  TMC5160Stepper leadDriver;

  // Applies the standard current and microstep settings for the lead-screw driver.
  void configureLeadDriver();

  // Keeps the interface compatible with the existing motion architecture.
  void enableMotor(Config::MotorId motor);

  // Maps a logical motor to the pin that toggles its step signal.
  uint8_t getStepPin(Config::MotorId motor) const;
  // Maps a logical motor to the pin that selects its movement direction.
  uint8_t getDirectionPin(Config::MotorId motor) const;

  // Calculates the delay between step pulses so the move accelerates and decelerates smoothly.
  uint16_t calculatePulseDelay(
    unsigned long stepNumber,
    unsigned long totalSteps,
    const Config::MotionProfile &profile
  ) const;

  // Checks whether the serial input contains an emergency-stop request.
  bool emergencyStopRequested();
};
