#include <math.h>

#include "Axes.h"

Axes::Axes(StepperController &stepperController)
  : stepper(stepperController),
    leadPositionSteps(0),
    leadPuckLevel(0),
    barrelPositionSteps(0),
    yawPositionSteps(0)
{
}

// Converts a lead-screw travel distance in millimeters into the matching motor step count.
long Axes::leadMillimetersToSteps(float millimeters) const {
  return lround(millimeters * Config::LEAD_STEPS_PER_MM);
}

// Converts a stored lead-screw step count back into travel distance in millimeters.
float Axes::leadStepsToMillimeters(long steps) const {
  if (Config::LEAD_STEPS_PER_MM <= 0.0f) {
    return 0.0f;
  }

  return (float)steps / Config::LEAD_STEPS_PER_MM;
}

// Returns the physical height of a given puck level above the bottom reference point.
float Axes::getLeadPuckLevelMillimeters(uint8_t puckLevel) const {
  if (puckLevel == 0) {
    return 0.0f;
  }

  return Config::FIRST_PUCK_EXTRA_OFFSET_MM +
    ((float)puckLevel * Config::PUCK_HEIGHT_MM);
}

// Moves the lead screw to an absolute step position while clamping it to the valid travel range.
void Axes::moveLeadToStepPosition(long targetStepPosition) {
  const long totalTravelSteps = leadMillimetersToSteps(
    Config::BARREL_HEIGHT_MM
  );

  if (targetStepPosition < 0) {
    targetStepPosition = 0;
  }

  if (targetStepPosition > totalTravelSteps) {
    targetStepPosition = totalTravelSteps;
  }

  long requestedMovement = targetStepPosition - leadPositionSteps;

  if (requestedMovement == 0) {
    Serial.println(F("Lead screw is already at that position."));
    return;
  }

  long completedMovement = stepper.moveSteps(
    Config::MotorId::LeadScrew,
    requestedMovement,
    Config::LEAD_POSITIVE_DIRECTION_LEVEL,
    Config::LEAD_PROFILE
  );

  leadPositionSteps += completedMovement;

  if (completedMovement == requestedMovement) {
    leadPositionSteps = targetStepPosition;
  }
  else {
    // a stopped move no longer matches a known puck level
    leadPuckLevel = -1;
    Serial.println(F("Lead-screw movement stopped before completion."));
  }

  Serial.print(F("Lead position: "));
  Serial.print(leadStepsToMillimeters(leadPositionSteps), 3);
  Serial.print(F(" / "));
  Serial.print(Config::BARREL_HEIGHT_MM, 3);
  Serial.print(F(" mm ("));
  Serial.print(leadPositionSteps);
  Serial.println(F(" steps)"));
}

// Moves the elevator to a named puck level and updates the tracked level state.
bool Axes::moveLeadToPuckLevel(uint8_t targetPuckLevel) {
  if (targetPuckLevel > Config::PUCK_COUNT) {
    Serial.println(F("All configured pucks have already been raised."));
    return false;
  }

  const float targetMillimeters =
    getLeadPuckLevelMillimeters(targetPuckLevel);

  if (targetMillimeters > Config::BARREL_HEIGHT_MM) {
    Serial.println(F("Configured puck stack plus offset exceeds barrel height."));
    return false;
  }

  long targetSteps = leadMillimetersToSteps(targetMillimeters);
  long startingPosition = leadPositionSteps;

  moveLeadToStepPosition(targetSteps);

  if (leadPositionSteps == targetSteps) {
    leadPuckLevel = (int8_t)targetPuckLevel;

    Serial.print(F("Lead puck level: "));
    Serial.print(leadPuckLevel);
    Serial.print(F(" / "));
    Serial.println(Config::PUCK_COUNT);

    return true;
  }

  if (leadPositionSteps != startingPosition) {
    leadPuckLevel = -1;
  }

  return false;
}

// Raises the elevator by one puck level when that move is still valid.
bool Axes::moveLeadUpOnePuck() {
  if (leadPuckLevel < 0) {
    Serial.println(F("Lead puck level is unknown. Home to bottom with D, then retry."));
    return false;
  }

  if (leadPuckLevel >= Config::PUCK_COUNT) {
    Serial.println(F("All configured pucks have already been raised."));
    return false;
  }

  return moveLeadToPuckLevel((uint8_t)(leadPuckLevel + 1));
}

// Lowers the elevator by one puck level when the current level is known.
bool Axes::moveLeadDownOnePuck() {
  if (leadPuckLevel < 0) {
    Serial.println(F("Lead puck level is unknown. Home to bottom with D, then retry."));
    return false;
  }

  if (leadPuckLevel == 0) {
    Serial.println(F("Lead screw is already at the bottom puck level."));
    return false;
  }

  return moveLeadToPuckLevel((uint8_t)(leadPuckLevel - 1));
}

// Moves the lead screw all the way to the top of its travel range.
void Axes::moveLeadToTop() {
  moveLeadToStepPosition(
    leadMillimetersToSteps(Config::BARREL_HEIGHT_MM)
  );

  // Full mechanical top is not necessarily one of the normal
  // puck deployment levels, so require re-homing before W/S.
  leadPuckLevel = -1;
}

// Drives the lead screw all the way down to the physical bottom stop, ignoring the remembered position.
void Axes::moveLeadToBottomFullDown() {
  const long fullTravelSteps = leadMillimetersToSteps(Config::BARREL_HEIGHT_MM);
  long remainingSteps = fullTravelSteps;

  Serial.println(F("Running lead screw fully down to the bottom stop..."));

  while (remainingSteps > 0) {
    if (digitalRead(Config::BOTTOM_LIMIT_SWITCH_PIN) == LOW) {
      break;
    }

    long chunkSteps = remainingSteps > 2000L ? 2000L : remainingSteps;
    long completedMovement = stepper.moveSteps(
      Config::MotorId::LeadScrew,
      -chunkSteps,
      Config::LEAD_POSITIVE_DIRECTION_LEVEL,
      Config::LEAD_PROFILE
    );

    if (completedMovement != -chunkSteps) {
      break;
    }

    remainingSteps += completedMovement;
  }

  if (digitalRead(Config::BOTTOM_LIMIT_SWITCH_PIN) == LOW) {
    leadPositionSteps = 0;
    leadPuckLevel = 0;
    Serial.println(F("Lead screw reached the physical bottom stop."));
  }
  else {
    leadPositionSteps = 0;
    leadPuckLevel = -1;
    Serial.println(F("Lead screw reached the configured travel limit without a bottom-switch trigger."));
  }
}

// Moves the lead screw back to the bottom reference position.
void Axes::moveLeadToBottom() {
  moveLeadToStepPosition(0);

  if (leadPositionSteps == 0) {
    leadPuckLevel = 0;
  }
}

// Resets the internal lead position and puck-level tracking to the current location.
void Axes::setLeadPositionAsBottom() {
  leadPositionSteps = 0;
  leadPuckLevel = 0;
  Serial.println(F("Current lead-screw position set as bottom (0 mm)."));
}

// Calculates how many microsteps are needed for one full revolution of the barrel motor.
long Axes::getBarrelStepsPerRevolution() const {
  float calculatedSteps =
    (float)Config::BARREL_MOTOR_FULL_STEPS_PER_REVOLUTION *
    (float)Config::BARREL_MICROSTEPS *
    Config::BARREL_GEAR_RATIO;

  return lround(calculatedSteps);
}

// Keeps the barrel position wrapped into a single revolution of step-space.
long Axes::normalizeBarrelPosition(long position) const {
  long stepsPerRevolution = getBarrelStepsPerRevolution();

  if (stepsPerRevolution <= 0) {
    return 0;
  }

  position %= stepsPerRevolution;

  if (position < 0) {
    position += stepsPerRevolution;
  }

  return position;
}

// Returns the chamber index that best matches the current barrel step position.
int Axes::getNearestBarrelIndex() const {
  long stepsPerRevolution = getBarrelStepsPerRevolution();

  if (stepsPerRevolution <= 0) {
    return 0;
  }

  long roundedIndex =
    (
      barrelPositionSteps * Config::BARREL_POSITION_COUNT +
      stepsPerRevolution / 2L
    ) /
    stepsPerRevolution;

  roundedIndex %= Config::BARREL_POSITION_COUNT;

  return (int)roundedIndex;
}

// Moves the barrel to a specific chamber index using the shortest valid rotation.
void Axes::moveBarrelToIndex(int targetIndex) {
  if (
    targetIndex < 0 ||
    targetIndex >= Config::BARREL_POSITION_COUNT
  ) {
    Serial.print(F("Barrel index must be between 0 and "));
    Serial.print(Config::BARREL_POSITION_COUNT - 1);
    Serial.println(F("."));
    return;
  }

  long stepsPerRevolution = getBarrelStepsPerRevolution();

  if (stepsPerRevolution <= 0) {
    Serial.println(F("Invalid barrel calibration."));
    return;
  }

  long targetStepPosition = lround(
    (
      (float)targetIndex *
      (float)stepsPerRevolution
    ) /
    (float)Config::BARREL_POSITION_COUNT
  );

  long requestedMovement = targetStepPosition - barrelPositionSteps;

  // take the shortest route to the requested chamber
  if (requestedMovement > stepsPerRevolution / 2L) {
    requestedMovement -= stepsPerRevolution;
  }
  else if (requestedMovement < -(stepsPerRevolution / 2L)) {
    requestedMovement += stepsPerRevolution;
  }

  Serial.print(F("Moving barrel to index "));
  Serial.println(targetIndex);

  long completedMovement = stepper.moveSteps(
    Config::MotorId::Barrel,
    requestedMovement,
    Config::BARREL_POSITIVE_DIRECTION_LEVEL,
    Config::BARREL_PROFILE
  );

  barrelPositionSteps = normalizeBarrelPosition(
    barrelPositionSteps + completedMovement
  );

  if (completedMovement == requestedMovement) {
    barrelPositionSteps = normalizeBarrelPosition(targetStepPosition);
    Serial.println(F("Barrel index movement complete."));
  }
  else {
    Serial.println(F("Barrel movement stopped before completion."));
  }
}

// Advances the barrel to the next chamber index.
void Axes::moveBarrelToNextIndex() {
  int nextIndex = getNearestBarrelIndex() + 1;

  if (nextIndex >= Config::BARREL_POSITION_COUNT) {
    nextIndex = 0;
  }

  moveBarrelToIndex(nextIndex);
}

// Moves the barrel back to the previous chamber index.
void Axes::moveBarrelToPreviousIndex() {
  int previousIndex = getNearestBarrelIndex() - 1;

  if (previousIndex < 0) {
    previousIndex = Config::BARREL_POSITION_COUNT - 1;
  }

  moveBarrelToIndex(previousIndex);
}

// Resets the barrel's internal position to the current chamber location.
void Axes::setBarrelPositionAsIndexZero() {
  barrelPositionSteps = 0;
  Serial.println(F("Current barrel position set as index 0."));
}

// Converts yaw angle into the equivalent step count for the configured motor.
float Axes::getYawStepsPerDegree() const {
  float outputStepsPerRevolution =
    (float)Config::YAW_MOTOR_FULL_STEPS_PER_REVOLUTION *
    (float)Config::YAW_MICROSTEPS *
    Config::YAW_GEAR_RATIO;

  return outputStepsPerRevolution / 360.0f;
}

// Converts one yaw step into a tiny angular change so the motion can be reported in degrees.
float Axes::getYawDegreesPerStep() const {
  float stepsPerDegree = getYawStepsPerDegree();

  if (stepsPerDegree <= 0.0f) {
    return 0.0f;
  }

  return 1.0f / stepsPerDegree;
}

// Returns the current yaw angle based on the internal step counter.
float Axes::getCurrentYawDegrees() const {
  float stepsPerDegree = getYawStepsPerDegree();

  if (stepsPerDegree <= 0.0f) {
    return 0.0f;
  }

  return (float)yawPositionSteps / stepsPerDegree;
}

// Moves the yaw axis to a target angle while clamping it to the allowed range.
void Axes::moveYawToDegrees(float targetDegrees) {
  if (targetDegrees < Config::YAW_MINIMUM_DEGREES) {
    targetDegrees = Config::YAW_MINIMUM_DEGREES;
  }

  if (targetDegrees > Config::YAW_MAXIMUM_DEGREES) {
    targetDegrees = Config::YAW_MAXIMUM_DEGREES;
  }

  long targetStepPosition = lround(
    targetDegrees * getYawStepsPerDegree()
  );

  long requestedMovement = targetStepPosition - yawPositionSteps;

  // round to the nearest physically commandable microstep
  float achievableTargetDegrees =
    (float)targetStepPosition * getYawDegreesPerStep();

  Serial.print(F("Moving yaw to requested "));
  Serial.print(targetDegrees, 4);
  Serial.print(F(" deg; achievable target "));
  Serial.print(achievableTargetDegrees, 6);
  Serial.println(F(" deg"));

  long completedMovement = stepper.moveSteps(
    Config::MotorId::Yaw,
    requestedMovement,
    Config::YAW_POSITIVE_DIRECTION_LEVEL,
    Config::YAW_PROFILE,
    Config::YAW_HOLD_AFTER_MOVE
  );

  yawPositionSteps += completedMovement;

  if (completedMovement == requestedMovement) {
    yawPositionSteps = targetStepPosition;
    Serial.println(F("Yaw movement complete."));
  }
  else {
    Serial.println(F("Yaw movement stopped before completion."));
  }
}

// Moves the yaw axis by a relative angle from the current position.
// Moves the yaw axis by a small relative offset from the current position.
void Axes::moveYawRelativeDegrees(float movementDegrees) {
  moveYawToDegrees(getCurrentYawDegrees() + movementDegrees);
}

// Moves the yaw axis by a raw microstep count while keeping it inside the mechanical limits.
void Axes::moveYawByMicrosteps(long microsteps) {
  if (microsteps == 0) {
    Serial.println(F("Yaw microstep movement must not be zero."));
    return;
  }

  long minimumStepPosition = lround(
    Config::YAW_MINIMUM_DEGREES * getYawStepsPerDegree()
  );

  long maximumStepPosition = lround(
    Config::YAW_MAXIMUM_DEGREES * getYawStepsPerDegree()
  );

  long targetStepPosition = yawPositionSteps + microsteps;

  if (targetStepPosition < minimumStepPosition) {
    targetStepPosition = minimumStepPosition;
  }

  if (targetStepPosition > maximumStepPosition) {
    targetStepPosition = maximumStepPosition;
  }

  long requestedMovement = targetStepPosition - yawPositionSteps;

  if (requestedMovement == 0) {
    Serial.println(F("Yaw is already at the requested travel limit."));
    return;
  }

  long completedMovement = stepper.moveSteps(
    Config::MotorId::Yaw,
    requestedMovement,
    Config::YAW_POSITIVE_DIRECTION_LEVEL,
    Config::YAW_PROFILE,
    Config::YAW_HOLD_AFTER_MOVE
  );

  yawPositionSteps += completedMovement;

  if (completedMovement == requestedMovement) {
    yawPositionSteps = targetStepPosition;
  }

  Serial.print(F("Yaw moved to "));
  Serial.print(getCurrentYawDegrees(), 6);
  Serial.print(F(" degrees ("));
  Serial.print(yawPositionSteps);
  Serial.println(F(" microsteps from zero)."));
}

// Disables the yaw motor so it no longer holds position.
void Axes::releaseYawMotor() {
  stepper.disableMotor(Config::MotorId::Yaw);
  Serial.println(F("Yaw motor released; holding torque is off."));
}

// Resets the yaw step counter to the current position as zero.
void Axes::setYawPositionAsZero() {
  yawPositionSteps = 0;
  Serial.println(F("Current yaw position set as 0 degrees."));
}

// Reports the current state of all axes and their configured limits.
void Axes::printStatus(Stream &output) const {
  output.println();
  output.println(F("========== POSITION STATUS =========="));

  output.print(F("Lead screw: "));
  output.print(leadStepsToMillimeters(leadPositionSteps), 3);
  output.print(F(" / "));
  output.print(Config::BARREL_HEIGHT_MM, 3);
  output.print(F(" mm ("));
  output.print(leadPositionSteps);
  output.println(F(" steps)"));

  output.print(F("Lead puck level: "));
  if (leadPuckLevel < 0) {
    output.println(F("unknown; home to bottom before W/S"));
  }
  else {
    output.print(leadPuckLevel);
    output.print(F(" / "));
    output.println(Config::PUCK_COUNT);
  }

  output.print(F("Barrel index: "));
  output.print(getNearestBarrelIndex());
  output.print(F(" of "));
  output.println(Config::BARREL_POSITION_COUNT);

  output.print(F("Barrel position: "));
  output.print(barrelPositionSteps);
  output.print(F(" / "));
  output.print(getBarrelStepsPerRevolution());
  output.println(F(" steps/revolution"));

  output.print(F("Yaw: "));
  output.print(getCurrentYawDegrees(), 6);
  output.println(F(" degrees"));

  output.print(F("Yaw resolution: "));
  output.print(getYawDegreesPerStep(), 8);
  output.println(F(" degrees per STEP pulse"));

  output.print(F("Yaw allowed range: "));
  output.print(Config::YAW_MINIMUM_DEGREES, 1);
  output.print(F(" to +"));
  output.print(Config::YAW_MAXIMUM_DEGREES, 1);
  output.println(F(" degrees"));

  output.print(F("Yaw position: "));
  output.print(yawPositionSteps);
  output.println(F(" steps"));

  output.println(F("====================================="));
}
