#pragma once

#include <Arduino.h>

#include "Config.h"
#include "Sensors.h"
#include "StepperController.h"

// Coordinates the lead screw, barrel, and yaw mechanisms for puck delivery.
class Axes {
public:
  // Creates the motion controller and stores references to the stepper driver and sensors.
  Axes(StepperController &stepperController, Sensors &sensorsRef);

  // Redefines the current yaw position as zero without moving.
  void setYawPositionAsZero();

  // Tracked elevator position and puck level.
  float getLeadPositionMillimeters() const;
  int8_t getLeadPuckLevel() const;

  // Lead-screw helpers. Moves update the tracked position/level in software.
  // moveLeadToBottom is software-only; moveLeadToBottomFullDown (B) ignores the tracked position.
  void moveLeadToBottom();
  // False if stopped before the switch or full-travel distance was reached.
  bool moveLeadToBottomFullDown();
  bool moveLeadToPuckLevel(uint8_t targetPuckLevel);
  bool moveLeadRelativeMillimeters(float millimeters);
  bool moveLeadUpOnePuck();
  bool moveLeadDownOnePuck();
  void moveLeadUpOneMillimeter();
  void moveLeadDownOneMillimeter();

  // Barrel helpers return false on invalid or interrupted moves. Indices wrap;
  // indexing runs with or without a solenoid.
  bool moveBarrelToIndex(int targetIndex);
  bool moveBarrelToNextIndex();
  bool moveBarrelToPreviousIndex();

  // Yaw helpers; angles are clamped to the configured yaw limits.
  void moveYawToDegrees(float targetDegrees);
  void moveYawRelativeDegrees(float movementDegrees);

  // Reports the current axis state to a serial stream.
  void printStatus(Stream &output) const;

private:
  StepperController &stepper;
  Sensors &sensors;

  long leadPositionSteps;
  int8_t leadPuckLevel;
  long barrelPositionSteps;
  long yawPositionSteps;

  // Returns the physical height of a given puck level above the bottom reference.
  float getLeadPuckLevelMillimeters(uint8_t puckLevel) const;
  // Calculates how many steps one full barrel revolution requires.
  long getBarrelStepsPerRevolution() const;
  // Finds the chamber index closest to the current barrel position.
  int getNearestBarrelIndex() const;
  // Converts yaw angle into the equivalent step count for the configured motor.
  float getYawStepsPerDegree() const;
  // Returns the current yaw angle from the internal step counter.
  float getCurrentYawDegrees() const;
  // Converts one step of yaw movement into a small angular change in degrees.
  float getYawDegreesPerStep() const;

  // Lead-screw conversions and motion helpers.
  long leadMillimetersToSteps(float millimeters) const;
  float leadStepsToMillimeters(long steps) const;
  int8_t inferLeadPuckLevelFromPosition(long positionSteps) const;
  void moveLeadToStepPosition(long targetStepPosition);
  bool applyLeadLimitEvent();

  // Wraps the barrel position into a single revolution.
  long normalizeBarrelPosition(long position) const;
};
