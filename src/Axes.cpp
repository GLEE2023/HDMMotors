#include <math.h>

#include "Axes.h"

Axes::Axes(StepperController &stepperController, Sensors &sensorsRef)
  : stepper(stepperController),
    sensors(sensorsRef),
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

  if (puckLevel == 1) {
    return Config::FIRST_PUCK_EXTRA_OFFSET_MM;
  }

  return Config::FIRST_PUCK_EXTRA_OFFSET_MM +
    ((float)(puckLevel - 1) * Config::PUCK_HEIGHT_MM);
}

// Infers the current puck level from the physical lead-screw position.
int8_t Axes::inferLeadPuckLevelFromPosition(long positionSteps) const {
  const float currentMillimeters = leadStepsToMillimeters(positionSteps);

  for (uint8_t puckLevel = Config::PUCK_COUNT; puckLevel > 0; --puckLevel) {
    if (currentMillimeters >= getLeadPuckLevelMillimeters(puckLevel)) {
      return (int8_t)puckLevel;
    }
  }

  return 0;
}

// Re-evaluates the lead position after the stepper reports a bottom microswitch stop.
bool Axes::applyLeadLimitEvent() {
  const StepperController::LeadLimit limit = stepper.takeLeadLimitEvent();

  if (limit == StepperController::LeadLimit::Bottom) {
    leadPositionSteps = 0;
    leadPuckLevel = 0;
    return true;
  }

  return false;
}

// Moves to an absolute tracked step position, clamped to 0..BARREL_HEIGHT_MM.
// Bottom switch stop -> position 0, level 0. Incomplete move -> level -1.
// A completed move leaves the puck level unchanged (callers set it).
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

  const bool limitTripped = applyLeadLimitEvent();

  if (limitTripped) {
    Serial.println(F("Lead-screw movement ended at a limit microswitch."));
  }
  else if (completedMovement == requestedMovement) {
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

// Moves to the height of a puck level (0 = bottom). Returns true only if the
// final tracked position equals the target; level is then set, or -1 if the lead
// moved but did not arrive.
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

// W command. If the level is unknown (-1) it is inferred from position first.
// Fails if already at the top level.
bool Axes::moveLeadUpOnePuck() {
  int8_t currentPuckLevel = leadPuckLevel;

  if (currentPuckLevel < 0) {
    currentPuckLevel = inferLeadPuckLevelFromPosition(leadPositionSteps);
  }

  if (currentPuckLevel >= Config::PUCK_COUNT) {
    Serial.println(F("All configured pucks have already been raised."));
    return false;
  }

  return moveLeadToPuckLevel((uint8_t)(currentPuckLevel + 1));
}

// S command. Level inferred from position if unknown; fails at level 0.
bool Axes::moveLeadDownOnePuck() {
  int8_t currentPuckLevel = leadPuckLevel;

  if (currentPuckLevel < 0) {
    currentPuckLevel = inferLeadPuckLevelFromPosition(leadPositionSteps);
  }

  if (currentPuckLevel == 0) {
    Serial.println(F("Lead screw is already at the bottom puck level."));
    return false;
  }

  return moveLeadToPuckLevel((uint8_t)(currentPuckLevel - 1));
}

// Relative lead move (clamped to 0..BARREL_HEIGHT_MM). A clamped move keeps the
// clamped position as the new reference. Returns true only if the unclamped
// target was reached (false when clamped). The puck level is re-inferred from
// position if the lead reached its clamped target, or set to -1 if the move
// was cut short.
bool Axes::moveLeadRelativeMillimeters(float millimeters) {
  if (millimeters == 0.0f) {
    return true;
  }

  const long requestedTarget = leadPositionSteps + leadMillimetersToSteps(millimeters);
  const long clampedTarget = constrain(
    requestedTarget,
    0L,
    leadMillimetersToSteps(Config::BARREL_HEIGHT_MM)
  );

  moveLeadToStepPosition(requestedTarget);

  leadPuckLevel = leadPositionSteps == clampedTarget
    ? inferLeadPuckLevelFromPosition(leadPositionSteps)
    : -1;

  return leadPositionSteps == requestedTarget;
}

// Moves the lead screw up by 1 mm for alignment checks.
void Axes::moveLeadUpOneMillimeter() {
  moveLeadRelativeMillimeters(1.0f);
}

// Moves the lead screw down by 1 mm for alignment checks.
void Axes::moveLeadDownOneMillimeter() {
  moveLeadRelativeMillimeters(-1.0f);
}

// Returns the currently tracked puck level for the lead screw.
int8_t Axes::getLeadPuckLevel() const {
  return leadPuckLevel;
}

// Returns the tracked lead-screw position relative to its bottom reference.
float Axes::getLeadPositionMillimeters() const {
  return leadStepsToMillimeters(leadPositionSteps);
}

// B command. Ignores the tracked position: drives down BARREL_HEIGHT_MM worth of
// steps in 2000-step chunks, stopping early if the bottom switch is tripped or a
// chunk is cut short by x/X. Afterwards position is set to 0 and level to 0 in
// every case: switch tripped, full travel done (assumed, unconfirmed), or
// stopped by x/X (the stop point is taken as the new zero).
void Axes::moveLeadToBottomFullDown() {
  long remainingSteps = leadMillimetersToSteps(Config::BARREL_HEIGHT_MM);

  Serial.println(F("Running lead screw down for the full configured travel..."));

  while (remainingSteps > 0 && !sensors.isLeadBottomSwitchTripped()) {
    const long requestedSteps = min(remainingSteps, 2000L);
    const long completedSteps = -stepper.moveSteps(
      Config::MotorId::LeadScrew,
      -requestedSteps,
      Config::LEAD_POSITIVE_DIRECTION_LEVEL,
      Config::LEAD_PROFILE
    );

    remainingSteps -= completedSteps;

    if (completedSteps != requestedSteps) {
      break;
    }
  }

  stepper.takeLeadLimitEvent();

  leadPositionSteps = 0;
  leadPuckLevel = 0;

  if (sensors.isLeadBottomSwitchTripped()) {
    Serial.println(F("Lead screw reached the physical bottom stop."));
  }
  else if (remainingSteps <= 0) {
    Serial.println(F("Bottom switch did not trip; assuming bottom from the step count."));
  }
  else {
    Serial.println(F("Lead screw stopped early; current position set as bottom (0 mm)."));
  }
}

// Software-only move to tracked position 0; limited to the tracked distance, so
// it does not reach the true bottom if the tracked position is wrong.
void Axes::moveLeadToBottom() {
  moveLeadToStepPosition(0);

  if (leadPositionSteps == 0) {
    leadPuckLevel = 0;
  }
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

// Rotates the barrel to an index by the shortest direction (barrel motor only).
// Runs even without a solenoid (warns); with one, unlocks before and locks after
// the move. Position is trusted only if the full move completed.
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

  const bool solenoidConfigured = sensors.hasBarrelSolenoid();
  if (solenoidConfigured) {
    sensors.unlockBarrel();
  }
  else {
    Serial.println(F("Barrel solenoid is not configured; indexing without unlocking."));
  }

  long completedMovement = stepper.moveSteps(
    Config::MotorId::Barrel,
    requestedMovement,
    Config::BARREL_POSITIVE_DIRECTION_LEVEL,
    Config::BARREL_PROFILE
  );

  sensors.lockBarrel();

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

// Moves yaw to an absolute angle, clamped to the configured limits (a clamp is
// reported). Yaw and barrel motors step together; the yaw motor is left enabled
// when YAW_HOLD_AFTER_MOVE is set. Tracked steps follow what was actually moved.
void Axes::moveYawToDegrees(float targetDegrees) {
  const float requestedDegrees = targetDegrees;

  targetDegrees = constrain(
    targetDegrees,
    Config::YAW_MINIMUM_DEGREES,
    Config::YAW_MAXIMUM_DEGREES
  );

  if (targetDegrees != requestedDegrees) {
    Serial.print(F("Requested yaw "));
    Serial.print(requestedDegrees, 4);
    Serial.println(F(" deg is outside the yaw limits; clamped."));
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

  long completedMovement = stepper.moveCoordinatedSteps(
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

// Yaw relative to the tracked angle; the result is clamped by moveYawToDegrees.
void Axes::moveYawRelativeDegrees(float movementDegrees) {
  moveYawToDegrees(getCurrentYawDegrees() + movementDegrees);
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
