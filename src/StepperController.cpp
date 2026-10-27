#include "StepperController.h"

StepperController::StepperController(Sensors &sensorsRef)
  : leadDriver(Config::LEAD_CHIP_SELECT_PIN, Config::R_SENSE),
    sensors(sensorsRef)
{}

// Sets up STEP/DIR outputs (all low) and optional enable pins (driven high =
// disabled), then starts SPI and configures the lead TMC5160 only. Barrel and
// yaw drivers are plain STEP/DIR; nothing is configured on them.
void StepperController::begin() {
  // Lead screw pins
  pinMode(Config::LEAD_STEP_PIN, OUTPUT);
  pinMode(Config::LEAD_DIRECTION_PIN, OUTPUT);

  // Barrel standalone TMC2209 STEP/DIR pins
  pinMode(Config::BARREL_STEP_PIN, OUTPUT);
  pinMode(Config::BARREL_DIRECTION_PIN, OUTPUT);

  // Yaw standalone TMC2209 STEP/DIR pins
  pinMode(Config::YAW_STEP_PIN, OUTPUT);
  pinMode(Config::YAW_DIRECTION_PIN, OUTPUT);

  if (Config::LEAD_ENABLE_PIN != Config::UNUSED_PIN) {
    pinMode(Config::LEAD_ENABLE_PIN, OUTPUT);
    digitalWrite(Config::LEAD_ENABLE_PIN, HIGH);
  }

  if (Config::BARREL_ENABLE_PIN != Config::UNUSED_PIN) {
    pinMode(Config::BARREL_ENABLE_PIN, OUTPUT);
    digitalWrite(Config::BARREL_ENABLE_PIN, HIGH);
  }

  if (Config::YAW_ENABLE_PIN != Config::UNUSED_PIN) {
    pinMode(Config::YAW_ENABLE_PIN, OUTPUT);
    digitalWrite(Config::YAW_ENABLE_PIN, HIGH);
  }

  // Initial STEP and DIR states
  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::LEAD_DIRECTION_PIN, LOW);

  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_DIRECTION_PIN, LOW);

  digitalWrite(Config::YAW_STEP_PIN, LOW);
  digitalWrite(Config::YAW_DIRECTION_PIN, LOW);

  // Only the lead-screw TMC5160 remains on SPI.
  pinMode(Config::LEAD_CHIP_SELECT_PIN, OUTPUT);
  digitalWrite(Config::LEAD_CHIP_SELECT_PIN, HIGH);

  SPI.begin();
  configureLeadDriver();
}

// Lead TMC5160 over SPI: SpreadCycle, configured current and microsteps.
// No return value or error check on the SPI writes.
void StepperController::configureLeadDriver() {
  leadDriver.begin();
  leadDriver.toff(5);
  leadDriver.rms_current(Config::LEAD_CURRENT_MA);
  leadDriver.microsteps(Config::LEAD_MICROSTEPS);

  // SpreadCycle mode
  leadDriver.en_pwm_mode(false);
}

// Calls disableMotor on all three motors; true only if every one had an enable pin.
bool StepperController::disableAllMotors() {
  const bool leadDisabled = disableMotor(Config::MotorId::LeadScrew);
  const bool barrelDisabled = disableMotor(Config::MotorId::Barrel);
  const bool yawDisabled = disableMotor(Config::MotorId::Yaw);

  return leadDisabled && barrelDisabled && yawDisabled;
}

// Pin lookups; UNUSED_PIN is returned for MotorId::None.
uint8_t StepperController::getStepPin(Config::MotorId motor) const {
  switch (motor) {
    case Config::MotorId::LeadScrew:
      return Config::LEAD_STEP_PIN;

    case Config::MotorId::Barrel:
      return Config::BARREL_STEP_PIN;

    case Config::MotorId::Yaw:
      return Config::YAW_STEP_PIN;

    case Config::MotorId::None:
    default:
      return Config::UNUSED_PIN;
  }
}

uint8_t StepperController::getDirectionPin(Config::MotorId motor) const {
  switch (motor) {
    case Config::MotorId::LeadScrew:
      return Config::LEAD_DIRECTION_PIN;

    case Config::MotorId::Barrel:
      return Config::BARREL_DIRECTION_PIN;

    case Config::MotorId::Yaw:
      return Config::YAW_DIRECTION_PIN;

    case Config::MotorId::None:
    default:
      return Config::UNUSED_PIN;
  }
}

// Returns UNUSED_PIN when the motor's enable pin is not configured.
uint8_t StepperController::getEnablePin(Config::MotorId motor) const {
  switch (motor) {
    case Config::MotorId::LeadScrew:
      return Config::LEAD_ENABLE_PIN;

    case Config::MotorId::Barrel:
      return Config::BARREL_ENABLE_PIN;

    case Config::MotorId::Yaw:
      return Config::YAW_ENABLE_PIN;

    case Config::MotorId::None:
    default:
      return Config::UNUSED_PIN;
  }
}

// Always sets STEP low. Drives the enable pin high (disabled, active-low enable)
// only if configured. Returns false if there is no enable pin (torque unchanged).
bool StepperController::disableMotor(Config::MotorId motor) {
  const uint8_t stepPin = getStepPin(motor);

  if (stepPin == Config::UNUSED_PIN) {
    return false;
  }

  digitalWrite(stepPin, LOW);

  const uint8_t enablePin = getEnablePin(motor);
  if (enablePin == Config::UNUSED_PIN) {
    return false;
  }

  digitalWrite(enablePin, HIGH);
  return true;
}

// Returns the limit event from the last lead move and clears it.
StepperController::LeadLimit StepperController::takeLeadLimitEvent() {
  LeadLimit event = leadLimitEvent;
  leadLimitEvent = LeadLimit::None;
  return event;
}

// Drives the enable pin low (enabled) and waits ENABLE_SETTLE_TIME_MS; does
// nothing if no enable pin is configured.
void StepperController::enableMotor(Config::MotorId motor) {
  const uint8_t enablePin = getEnablePin(motor);

  if (enablePin != Config::UNUSED_PIN) {
    digitalWrite(enablePin, LOW);
    delay(Config::ENABLE_SETTLE_TIME_MS);
  }
}

// Delay (us) after step stepNumber. Fixed cruise delay if the profile has no
// ramp or start <= cruise; otherwise a linear ramp from start to cruise over
// min(accelerationSteps, totalSteps/2) steps at both the start and the end.
uint16_t StepperController::calculatePulseDelay(
  unsigned long stepNumber,
  unsigned long totalSteps,
  const Config::MotionProfile &profile
) const {
  // For the lead screw, use the fixed delay from the working test code.
  if (profile.accelerationSteps == 0) {
    return profile.cruiseDelayUs;
  }

  if (
    profile.startingDelayUs <= profile.cruiseDelayUs ||
    totalSteps < 2
  ) {
    return profile.cruiseDelayUs;
  }

  unsigned long rampSteps = profile.accelerationSteps;
  unsigned long halfMovement = totalSteps / 2UL;

  if (rampSteps > halfMovement) {
    rampSteps = halfMovement;
  }

  if (rampSteps == 0) {
    return profile.cruiseDelayUs;
  }

  unsigned long distanceFromStart = stepNumber;
  unsigned long distanceFromEnd =
    totalSteps - stepNumber - 1UL;

  unsigned long distanceFromNearestEnd = distanceFromStart;

  if (distanceFromEnd < distanceFromNearestEnd) {
    distanceFromNearestEnd = distanceFromEnd;
  }

  if (distanceFromNearestEnd >= rampSteps) {
    return profile.cruiseDelayUs;
  }

  unsigned long delayRange =
    (unsigned long)profile.startingDelayUs -
    (unsigned long)profile.cruiseDelayUs;

  unsigned long delayReduction =
    (delayRange * distanceFromNearestEnd) /
    rampSteps;

  return (uint16_t)(
    (unsigned long)profile.startingDelayUs -
    delayReduction
  );
}

// True if the next serial byte is x/X; that byte plus following CR/LF is
// consumed. Any other pending byte is left in the buffer (not consumed).
bool StepperController::emergencyStopRequested() {
  if (Serial.available() <= 0) {
    return false;
  }

  char incomingCharacter = (char)Serial.peek();

  if (incomingCharacter != 'x' &&
      incomingCharacter != 'X') {
    return false;
  }

  Serial.read();

  while (Serial.available() > 0) {
    char leftover = (char)Serial.peek();

    if (leftover == '\r' || leftover == '\n')
    {
      Serial.read();
    }
    else
    {
      break;
    }
  }

  Serial.println();
  Serial.println(F("EMERGENCY STOP"));

  return true;
}

// Blocking move of one motor by signedSteps. Returns signed steps actually done
// (0 if invalid, or a lead down-move starts with the bottom switch tripped).
// Checks for x/X every 16 steps. Lead only: latches the puck switch each step
// and stops on the bottom switch (sets leadLimitEvent). Lead is never disabled
// afterwards; other motors are disabled unless keepEnabledAfterMove.
long StepperController::moveSteps(
  Config::MotorId motor,
  long signedSteps,
  bool positiveDirectionLevel,
  const Config::MotionProfile &profile,
  bool keepEnabledAfterMove
) {
  if (
    motor == Config::MotorId::None ||
    signedSteps == 0
  ) {
    return 0;
  }

  uint8_t stepPin = getStepPin(motor);
  uint8_t directionPin = getDirectionPin(motor);

  if (stepPin == Config::UNUSED_PIN || directionPin == Config::UNUSED_PIN) {
    return 0;
  }

  bool movingPositive = signedSteps > 0;

  bool directionLevel = movingPositive
    ? positiveDirectionLevel
    : !positiveDirectionLevel;

  unsigned long totalSteps = movingPositive
    ? (unsigned long)signedSteps
    : (unsigned long)(-signedSteps);

  const bool isLead = motor == Config::MotorId::LeadScrew;

  if (isLead) {
    leadLimitEvent = LeadLimit::None;
  }

  // Only downward motion is limited by a switch; the top switch just detects a puck.
  auto leadLimitHit = [&]() -> LeadLimit {
    if (!isLead) {
      return LeadLimit::None;
    }

    if (!movingPositive && sensors.isLeadBottomSwitchTripped()) {
      return LeadLimit::Bottom;
    }

    return LeadLimit::None;
  };

  digitalWrite(stepPin, LOW);

  // Refuse to drive further into a limit that is already tripped.
  if (leadLimitHit() != LeadLimit::None) {
    leadLimitEvent = leadLimitHit();
    return 0;
  }

  digitalWrite(
    directionPin,
    directionLevel ? HIGH : LOW
  );

  // Match the working standalone test more closely.
  delay(Config::DIRECTION_SETUP_DELAY_MS);

  enableMotor(motor);

  unsigned long completedSteps = 0;

  for (
    unsigned long stepNumber = 0;
    stepNumber < totalSteps;
    stepNumber++
  ) {
    if ((stepNumber & 0x0FUL) == 0UL) {
      if (emergencyStopRequested()) {
        break;
      }
    }

    if (isLead) {
      // Remember a puck trip even if the switch opens again before the move ends.
      sensors.updatePuckLatch();

      LeadLimit hit = leadLimitHit();

      if (hit != LeadLimit::None) {
        leadLimitEvent = hit;
        Serial.println(F("Bottom microswitch tripped; lead screw stopped."));
        break;
      }
    }

    uint16_t pulseDelay = calculatePulseDelay(
      stepNumber,
      totalSteps,
      profile
    );

    digitalWrite(stepPin, HIGH);
    delayMicroseconds(Config::STEP_HIGH_TIME_US);

    digitalWrite(stepPin, LOW);
    delayMicroseconds(pulseDelay);

    completedSteps++;
  }

  digitalWrite(stepPin, LOW);

  if (
    motor != Config::MotorId::LeadScrew &&
    !keepEnabledAfterMove
  ) {
    if (!disableMotor(motor)) {
      Serial.println(F("Motor enable pin is not configured; holding torque remains on."));
    }
  }

  return movingPositive
    ? (long)completedSteps
    : -(long)completedSteps;
}

// Barrel and yaw are stepped together on one shared pulse (both motors always
// move; barrel's partner runs in the opposite direction, yaw's the same).
// Enables both, no limit switches, x/X check every 16 steps, and returns signed
// steps done. Any other motor is passed to moveSteps.
long StepperController::moveCoordinatedSteps(
  Config::MotorId motor,
  long signedSteps,
  bool positiveDirectionLevel,
  const Config::MotionProfile &profile,
  bool keepEnabledAfterMove
) {
  if (signedSteps == 0) {
    return 0;
  }

  if (motor != Config::MotorId::Barrel && motor != Config::MotorId::Yaw) {
    return moveSteps(
      motor,
      signedSteps,
      positiveDirectionLevel,
      profile,
      keepEnabledAfterMove
    );
  }

  const bool movingPositive = signedSteps > 0;
  const bool directionLevel = movingPositive
    ? positiveDirectionLevel
    : !positiveDirectionLevel;

  // The partner motor follows the commanded one; barrel moves drive the yaw
  // motor in the opposite direction, yaw moves drive the barrel the same way.
  const bool isBarrel = motor == Config::MotorId::Barrel;
  const Config::MotorId partner = isBarrel
    ? Config::MotorId::Yaw
    : Config::MotorId::Barrel;
  const bool secondaryDirectionLevel = isBarrel
    ? !directionLevel
    : directionLevel;

  const uint8_t primaryStepPin = getStepPin(motor);
  const uint8_t primaryDirectionPin = getDirectionPin(motor);
  const uint8_t secondaryStepPin = getStepPin(partner);
  const uint8_t secondaryDirectionPin = getDirectionPin(partner);

  unsigned long totalSteps = movingPositive
    ? (unsigned long)signedSteps
    : (unsigned long)(-signedSteps);

  digitalWrite(primaryStepPin, LOW);
  digitalWrite(secondaryStepPin, LOW);
  digitalWrite(
    primaryDirectionPin,
    directionLevel ? HIGH : LOW
  );
  digitalWrite(
    secondaryDirectionPin,
    secondaryDirectionLevel ? HIGH : LOW
  );

  delay(Config::DIRECTION_SETUP_DELAY_MS);

  enableMotor(Config::MotorId::Barrel);
  enableMotor(Config::MotorId::Yaw);

  unsigned long completedSteps = 0;

  for (
    unsigned long stepNumber = 0;
    stepNumber < totalSteps;
    stepNumber++
  ) {
    if ((stepNumber & 0x0FUL) == 0UL) {
      if (emergencyStopRequested()) {
        break;
      }
    }

    uint16_t pulseDelay = calculatePulseDelay(
      stepNumber,
      totalSteps,
      profile
    );

    digitalWrite(primaryStepPin, HIGH);
    digitalWrite(secondaryStepPin, HIGH);
    delayMicroseconds(Config::STEP_HIGH_TIME_US);

    digitalWrite(primaryStepPin, LOW);
    digitalWrite(secondaryStepPin, LOW);
    delayMicroseconds(pulseDelay);

    completedSteps++;
  }

  digitalWrite(primaryStepPin, LOW);
  digitalWrite(secondaryStepPin, LOW);

  if (!keepEnabledAfterMove) {
    const bool barrelDisabled = disableMotor(Config::MotorId::Barrel);
    const bool yawDisabled = disableMotor(Config::MotorId::Yaw);

    if (!barrelDisabled || !yawDisabled) {
      Serial.println(F("Barrel/yaw enable pins are not both configured; holding torque may remain on."));
    }
  }

  return movingPositive
    ? (long)completedSteps
    : -(long)completedSteps;
}

// Prints only the lead TMC5160 SPI result (0 normally means OK); barrel and yaw
// cannot be tested.
void StepperController::printConnectionTests(Stream &output) {
  output.println();
  output.println(F("Stepper-driver checks:"));

  output.print(F(" Lead TMC5160 SPI result: "));
  output.println(leadDriver.test_connection());
  output.println(F("  A result of 0 normally indicates SPI success."));

  output.println(F(" Barrel TMC2209: standalone STEP/DIR; no UART test."));
  output.println(F(" Yaw TMC2209: standalone STEP/DIR; no UART test."));
}