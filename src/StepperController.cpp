#include "StepperController.h"

StepperController::StepperController()
  : leadDriver(Config::LEAD_CHIP_SELECT_PIN, Config::R_SENSE)
{}

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

void StepperController::configureLeadDriver() {
  leadDriver.begin();
  leadDriver.toff(5);
  leadDriver.rms_current(Config::LEAD_CURRENT_MA);
  leadDriver.microsteps(Config::LEAD_MICROSTEPS);

  // SpreadCycle mode
  leadDriver.en_pwm_mode(false);
}

void StepperController::disableAllMotors() {
  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::YAW_STEP_PIN, LOW);
}

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
      return 255;
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
      return 255;
  }
}

void StepperController::disableMotor(Config::MotorId motor) {
  uint8_t stepPin = getStepPin(motor);

  if (stepPin != 255) {
    digitalWrite(stepPin, LOW);
  }
}

void StepperController::enableMotor(Config::MotorId motor) {
  (void)motor;
}

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

  if (stepPin == 255 || directionPin == 255) {
    return 0;
  }

  bool movingPositive = signedSteps > 0;

  bool directionLevel = movingPositive
    ? positiveDirectionLevel
    : !positiveDirectionLevel;

  unsigned long totalSteps = movingPositive
    ? (unsigned long)signedSteps
    : (unsigned long)(-signedSteps);

  digitalWrite(stepPin, LOW);

  digitalWrite(
    directionPin,
    directionLevel ? HIGH : LOW
  );

  // Match the working standalone test more closely.
  delay(5);

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

    uint16_t pulseDelay = calculatePulseDelay(
      stepNumber,
      totalSteps,
      profile
    );

    digitalWrite(stepPin, HIGH);
    delayMicroseconds(5);

    digitalWrite(stepPin, LOW);
    delayMicroseconds(pulseDelay);

    completedSteps++;
  }

  digitalWrite(stepPin, LOW);

  if (
    motor != Config::MotorId::LeadScrew &&
    !keepEnabledAfterMove
  ) {
    disableMotor(motor);
  }

  return movingPositive
    ? (long)completedSteps
    : -(long)completedSteps;
}

long StepperController::moveCoordinatedSteps(
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

  bool movingPositive = signedSteps > 0;
  bool directionLevel = movingPositive
    ? positiveDirectionLevel
    : !positiveDirectionLevel;

  uint8_t primaryStepPin = 255;
  uint8_t primaryDirectionPin = 255;
  uint8_t secondaryStepPin = 255;
  uint8_t secondaryDirectionPin = 255;
  bool secondaryDirectionLevel = directionLevel;

  switch (motor) {
    case Config::MotorId::Barrel:
      primaryStepPin = getStepPin(Config::MotorId::Barrel);
      primaryDirectionPin = getDirectionPin(Config::MotorId::Barrel);
      secondaryStepPin = getStepPin(Config::MotorId::Yaw);
      secondaryDirectionPin = getDirectionPin(Config::MotorId::Yaw);
      secondaryDirectionLevel = !directionLevel;
      break;

    case Config::MotorId::Yaw:
      primaryStepPin = getStepPin(Config::MotorId::Yaw);
      primaryDirectionPin = getDirectionPin(Config::MotorId::Yaw);
      secondaryStepPin = getStepPin(Config::MotorId::Barrel);
      secondaryDirectionPin = getDirectionPin(Config::MotorId::Barrel);
      secondaryDirectionLevel = directionLevel;
      break;

    default:
      return moveSteps(
        motor,
        signedSteps,
        positiveDirectionLevel,
        profile,
        keepEnabledAfterMove
      );
  }

  if (
    primaryStepPin == 255 ||
    primaryDirectionPin == 255 ||
    secondaryStepPin == 255 ||
    secondaryDirectionPin == 255
  ) {
    return 0;
  }

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

  delay(5);

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

    uint16_t pulseDelay = calculatePulseDelay(
      stepNumber,
      totalSteps,
      profile
    );

    digitalWrite(primaryStepPin, HIGH);
    digitalWrite(secondaryStepPin, HIGH);
    delayMicroseconds(5);

    digitalWrite(primaryStepPin, LOW);
    digitalWrite(secondaryStepPin, LOW);
    delayMicroseconds(pulseDelay);

    completedSteps++;
  }

  digitalWrite(primaryStepPin, LOW);
  digitalWrite(secondaryStepPin, LOW);

  if (!keepEnabledAfterMove) {
    disableMotor(Config::MotorId::Barrel);
    disableMotor(Config::MotorId::Yaw);
  }

  return movingPositive
    ? (long)completedSteps
    : -(long)completedSteps;
}

void StepperController::printConnectionTests(Stream &output) {
  output.println();
  output.println(F("Stepper-driver checks:"));

  output.print(F(" Lead TMC5160 SPI result: "));
  output.println(leadDriver.test_connection());
  output.println(F("  A result of 0 normally indicates SPI success."));

  output.println(F(" Barrel TMC2209: standalone STEP/DIR; no UART test."));
  output.println(F(" Yaw TMC2209: standalone STEP/DIR; no UART test."));
}