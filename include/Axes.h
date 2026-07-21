#pragma once

#include <Arduino.h>

#include "Config.h"
#include "StepperController.h"

// Coordinates the lead screw, barrel, and yaw mechanisms for puck delivery.
class Axes {
public:
  // Creates the motion controller and stores a reference to the stepper driver.
  explicit Axes(StepperController &stepperController);

  // Lead-screw helpers for moving the elevator between puck levels.
  bool moveLeadUpOnePuck();
  bool moveLeadDownOnePuck();
  void moveLeadToTop();
  void moveLeadToBottom();
  void setLeadPositionAsBottom();

  // Barrel helpers for rotating the chamber to the next or previous firing position.
  void moveBarrelToNextIndex();
  void moveBarrelToPreviousIndex();
  void moveBarrelToIndex(int targetIndex);
  void setBarrelPositionAsIndexZero();

  // Yaw helpers for aiming the firing angle within the configured mechanical limits.
  void moveYawToDegrees(float targetDegrees);
  void moveYawRelativeDegrees(float movementDegrees);
  void moveYawByMicrosteps(long microsteps);
  void releaseYawMotor();
  void setYawPositionAsZero();

  // Reports the current axis state to a serial stream.
  void printStatus(Stream &output) const;

private:
  StepperController &stepper;

  long leadPositionSteps;
  int8_t leadPuckLevel;
  long barrelPositionSteps;
  long yawPositionSteps;

  // Converts lead-screw travel in millimeters into the matching step count.
  long leadMillimetersToSteps(float millimeters) const;
  // Converts a stored step count back into lead-screw travel in millimeters.
  float leadStepsToMillimeters(long steps) const;
  // Returns the physical height of a given puck level above the bottom reference.
  float getLeadPuckLevelMillimeters(uint8_t puckLevel) const;
  // Moves the lead screw directly to an absolute step position.
  void moveLeadToStepPosition(long targetStepPosition);
  // Moves the elevator to a named puck level and updates its internal state.
  bool moveLeadToPuckLevel(uint8_t targetPuckLevel);

  // Calculates how many steps one full barrel revolution requires.
  long getBarrelStepsPerRevolution() const;
  // Wraps the barrel position so it remains in a single revolution of step-space.
  long normalizeBarrelPosition(long position) const;
  // Finds the chamber index closest to the current barrel position.
  int getNearestBarrelIndex() const;

  // Converts yaw angle into the equivalent step count for the configured motor.
  float getYawStepsPerDegree() const;
  // Returns the current yaw angle from the internal step counter.
  float getCurrentYawDegrees() const;
  // Converts one step of yaw movement into a small angular change in degrees.
  float getYawDegreesPerStep() const;
};
