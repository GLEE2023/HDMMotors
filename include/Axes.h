#pragma once

#include <Arduino.h>

#include "Config.h"
#include "StepperController.h"

class Axes
{
public:
  explicit Axes(StepperController &stepperController);

  bool moveLeadUpOnePuck();
  bool moveLeadDownOnePuck();
  void moveLeadToTop();
  void moveLeadToBottom();
  void setLeadPositionAsBottom();

  void moveBarrelToNextIndex();
  void moveBarrelToPreviousIndex();
  void moveBarrelToIndex(int targetIndex);
  void setBarrelPositionAsIndexZero();

  void moveYawToDegrees(float targetDegrees);
  void moveYawRelativeDegrees(float movementDegrees);
  void moveYawByMicrosteps(long microsteps);
  void releaseYawMotor();
  void setYawPositionAsZero();

  void printStatus(Stream &output) const;

private:
  StepperController &stepper;

  long leadPositionSteps;
  int8_t leadPuckLevel;
  long barrelPositionSteps;
  long yawPositionSteps;

  long leadMillimetersToSteps(float millimeters) const;
  float leadStepsToMillimeters(long steps) const;
  float getLeadPuckLevelMillimeters(uint8_t puckLevel) const;
  void moveLeadToStepPosition(long targetStepPosition);
  bool moveLeadToPuckLevel(uint8_t targetPuckLevel);

  long getBarrelStepsPerRevolution() const;
  long normalizeBarrelPosition(long position) const;
  int getNearestBarrelIndex() const;

  float getYawStepsPerDegree() const;
  float getCurrentYawDegrees() const;
  float getYawDegreesPerStep() const;
};
