#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <TMCStepper.h>

#include "Config.h"
#include "Sensors.h"

// Low-level driver wrapper for the motion system.
// The lead screw remains a TMC5160 over SPI, while barrel and yaw use
// standalone TMC2209 STEP/DIR drivers without UART.
class StepperController {
public:
  StepperController(Sensors &sensorsRef);

  // Initializes pins, SPI, and the configured lead-screw driver.
  void begin();
  // Stops every motor's pulses; returns false if an enable pin is unconfigured.
  bool disableAllMotors();
  // Stops a motor's pulses; also disables it only if an enable pin is configured (returns false otherwise).
  bool disableMotor(Config::MotorId motor);

  // Blocking single-motor move; returns signed steps done. See the .cpp for lead-only limit handling.
  long moveSteps(
    Config::MotorId motor,
    long signedSteps,
    bool positiveDirectionLevel,
    const Config::MotionProfile &profile,
    bool keepEnabledAfterMove = false
  );

  // Barrel and yaw step together on shared pulses (both always move); other motors use moveSteps.
  long moveCoordinatedSteps(
    Config::MotorId motor,
    long signedSteps,
    bool positiveDirectionLevel,
    const Config::MotionProfile &profile,
    bool keepEnabledAfterMove = false
  );

  // Prints the lead TMC5160 SPI result; barrel/yaw are not tested.
  void printConnectionTests(Stream &output);

  // Which lead-screw limit switch stopped the most recent lead move.
  enum class LeadLimit : uint8_t {
    None,
    Bottom
  };

  // Returns the limit that halted the last lead move and clears it.
  LeadLimit takeLeadLimitEvent();

private:
  TMC5160Stepper leadDriver;
  Sensors &sensors;
  LeadLimit leadLimitEvent = LeadLimit::None;

  // Applies the standard current and microstep settings for the lead-screw driver.
  void configureLeadDriver();

  // Drives the enable pin low (if configured) and waits for it to settle.
  void enableMotor(Config::MotorId motor);

  // Maps a logical motor to the pin that toggles its step signal.
  uint8_t getStepPin(Config::MotorId motor) const;
  // Maps a logical motor to the pin that selects its movement direction.
  uint8_t getDirectionPin(Config::MotorId motor) const;
  // Maps a logical motor to its optional active-low enable pin.
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
