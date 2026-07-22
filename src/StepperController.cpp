#include "StepperController.h"

StepperController::StepperController()
  : leadDriver(Config::LEAD_CHIP_SELECT_PIN, Config::R_SENSE),
    barrelDriver(Config::BARREL_CHIP_SELECT_PIN, Config::R_SENSE),
    yawDriver(Config::YAW_CHIP_SELECT_PIN, Config::R_SENSE)
{}

void StepperController::begin() {
  // Lead screw pins
  pinMode(Config::LEAD_STEP_PIN, OUTPUT);
  pinMode(Config::LEAD_DIRECTION_PIN, OUTPUT);
  pinMode(Config::LEAD_ENABLE_PIN, OUTPUT);

  // Barrel pins
  pinMode(Config::BARREL_STEP_PIN, OUTPUT);
  pinMode(Config::BARREL_DIRECTION_PIN, OUTPUT);
  pinMode(Config::BARREL_ENABLE_PIN, OUTPUT);

  // Yaw pins
  pinMode(Config::YAW_STEP_PIN, OUTPUT);
  pinMode(Config::YAW_DIRECTION_PIN, OUTPUT);
  pinMode(Config::YAW_ENABLE_PIN, OUTPUT);

  // SPI chip-select pins
  pinMode(Config::LEAD_CHIP_SELECT_PIN, OUTPUT);
  pinMode(Config::BARREL_CHIP_SELECT_PIN, OUTPUT);
  pinMode(Config::YAW_CHIP_SELECT_PIN, OUTPUT);

  // Initial STEP and DIR states
  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::LEAD_DIRECTION_PIN, LOW);

  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_DIRECTION_PIN, LOW);

  digitalWrite(Config::YAW_STEP_PIN, LOW);
  digitalWrite(Config::YAW_DIRECTION_PIN, LOW);

  // Keep the lead screw permanently enabled.
  // TMC5160 enable is active-low.
  digitalWrite(Config::LEAD_ENABLE_PIN, LOW);

  // Keep barrel and yaw disabled during this lead-screw test.
  digitalWrite(Config::BARREL_ENABLE_PIN, HIGH);
  digitalWrite(Config::YAW_ENABLE_PIN, HIGH);

  // Deselect all SPI devices before SPI starts.
  digitalWrite(Config::LEAD_CHIP_SELECT_PIN, HIGH);
  digitalWrite(Config::BARREL_CHIP_SELECT_PIN, HIGH);
  digitalWrite(Config::YAW_CHIP_SELECT_PIN, HIGH);

  SPI.begin();

  configureDriver(
    leadDriver,
    Config::LEAD_CURRENT_MA,
    Config::LEAD_MICROSTEPS
  );

  configureDriver(
      barrelDriver,
      Config::BARREL_CURRENT_MA,
      Config::BARREL_MICROSTEPS
  );

  configureDriver(
      yawDriver,
      Config::YAW_CURRENT_MA,
      Config::YAW_MICROSTEPS
  );

  // Lead screw remains enabled so it holds the puck stack.
  digitalWrite(Config::LEAD_ENABLE_PIN, LOW);

  // Barrel and yaw remain disabled until a movement command.
  digitalWrite(Config::BARREL_ENABLE_PIN, HIGH);
  digitalWrite(Config::YAW_ENABLE_PIN, HIGH);
}

void StepperController::configureDriver(
  TMC5160Stepper &driver,
  uint16_t currentMilliamps,
  uint16_t microsteps
) {
  driver.begin();
  driver.toff(5);
  driver.rms_current(currentMilliamps);
  driver.microsteps(microsteps);

  // SpreadCycle mode
  driver.en_pwm_mode(false);
}

void StepperController::disableAllMotors() {
  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::YAW_STEP_PIN, LOW);

  // Lead stays enabled.
  digitalWrite(Config::LEAD_ENABLE_PIN, LOW);

  // Barrel and yaw stay disabled.
  digitalWrite(Config::BARREL_ENABLE_PIN, HIGH);
  digitalWrite(Config::YAW_ENABLE_PIN, HIGH);
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
      return 255;
  }
}

void StepperController::disableMotor(Config::MotorId motor) {
  uint8_t stepPin = getStepPin(motor);

  if (stepPin != 255) {
    digitalWrite(stepPin, LOW);
  }

  // Never disable the lead-screw driver during this test.
  if (motor == Config::MotorId::LeadScrew) {
    digitalWrite(Config::LEAD_ENABLE_PIN, LOW);
    return;
  }

  uint8_t enablePin = getEnablePin(motor);

  if (enablePin != 255) {
    digitalWrite(enablePin, HIGH);
  }
}

void StepperController::enableMotor(Config::MotorId motor) {
  uint8_t enablePin = getEnablePin(motor);

  if (enablePin != 255) {
    digitalWrite(enablePin, LOW);
    delay(5);
  }
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

  // Keep lead screw enabled permanently.
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

void StepperController::printConnectionTests(Stream &output)
{
    output.println();
    output.println(F("TMC5160 SPI connection tests:"));

    output.print(F(" Lead screw: "));
    output.println(leadDriver.test_connection());

    output.print(F(" Barrel: "));
    output.println(barrelDriver.test_connection());

    output.print(F(" Yaw: "));
    output.println(yawDriver.test_connection());

    output.println(F("A result of 0 normally indicates success."));
}