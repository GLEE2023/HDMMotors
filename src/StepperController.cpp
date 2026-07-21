#include "StepperController.h"

StepperController::StepperController()
  : leadDriver(Config::LEAD_CHIP_SELECT_PIN, Config::R_SENSE),
    barrelDriver(Config::BARREL_CHIP_SELECT_PIN, Config::R_SENSE),
    yawDriver(Config::YAW_CHIP_SELECT_PIN, Config::R_SENSE)
{}

// Initializes all GPIO pins, SPI, and TMC5160 driver settings for the motion system.
void StepperController::begin() {
  pinMode(Config::LEAD_STEP_PIN, OUTPUT);
  pinMode(Config::LEAD_DIRECTION_PIN, OUTPUT);
  pinMode(Config::LEAD_ENABLE_PIN, OUTPUT);

  pinMode(Config::BARREL_STEP_PIN, OUTPUT);
  pinMode(Config::BARREL_DIRECTION_PIN, OUTPUT);
  pinMode(Config::BARREL_ENABLE_PIN, OUTPUT);

  pinMode(Config::YAW_STEP_PIN, OUTPUT);
  pinMode(Config::YAW_DIRECTION_PIN, OUTPUT);
  pinMode(Config::YAW_ENABLE_PIN, OUTPUT);

  pinMode(Config::LEAD_CHIP_SELECT_PIN, OUTPUT);
  pinMode(Config::BARREL_CHIP_SELECT_PIN, OUTPUT);
  pinMode(Config::YAW_CHIP_SELECT_PIN, OUTPUT);

  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::LEAD_DIRECTION_PIN, LOW);

  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_DIRECTION_PIN, LOW);

  digitalWrite(Config::YAW_STEP_PIN, LOW);
  digitalWrite(Config::YAW_DIRECTION_PIN, LOW);

  disableAllMotors();

  // Keep every SPI device deselected until addressed.
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

  disableAllMotors();
}

// Configures one TMC5160 driver with the requested current and microstep settings for this machine.
void StepperController::configureDriver(
  TMC5160Stepper &driver,
  uint16_t currentMilliamps,
  uint16_t microsteps
) {
  driver.begin();
  driver.toff(5);
  driver.rms_current(currentMilliamps);
  driver.microsteps(microsteps);

  // false selects SpreadCycle through the TMCStepper API.
  driver.en_pwm_mode(false);
}

// Disables every motor output stage and leaves each step line low to avoid accidental motion.
void StepperController::disableAllMotors() {
  // Keep every STEP line low while the motors are inactive.
  digitalWrite(Config::LEAD_STEP_PIN, LOW);
  digitalWrite(Config::BARREL_STEP_PIN, LOW);
  digitalWrite(Config::YAW_STEP_PIN, LOW);

  // TMC5160 enable is active-low. HIGH disables the output stage.
  digitalWrite(Config::LEAD_ENABLE_PIN, HIGH);
  digitalWrite(Config::BARREL_ENABLE_PIN, HIGH);
  digitalWrite(Config::YAW_ENABLE_PIN, HIGH);

  delayMicroseconds(Config::DIRECTION_SETUP_TIME_US);
}

// Maps a logical motor identity to its step-control pin.
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

// Maps a logical motor identity to its direction-control pin.
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

// Maps a logical motor identity to its enable-control pin.
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

// Disables one motor without changing the state of the other axes.
void StepperController::disableMotor(Config::MotorId motor) {
  uint8_t stepPin = getStepPin(motor);
  uint8_t enablePin = getEnablePin(motor);

  if (stepPin != 255) {
    digitalWrite(stepPin, LOW);
  }

  if (enablePin != 255) {
    digitalWrite(enablePin, HIGH);
  }
}

// Enables one motor and waits briefly for the driver to settle before sending steps.
void StepperController::enableMotor(Config::MotorId motor) {
  uint8_t enablePin = getEnablePin(motor);

  if (enablePin != 255) {
    digitalWrite(enablePin, LOW);
    delay(Config::ENABLE_SETTLE_TIME_MS);
  }
}

uint16_t StepperController::calculatePulseDelay(
  unsigned long stepNumber,
  unsigned long totalSteps,
  const Config::MotionProfile &profile
) const {
  if (
    profile.accelerationSteps == 0 ||
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
  unsigned long distanceFromEnd = totalSteps - stepNumber - 1UL;
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
    (unsigned long)profile.startingDelayUs - delayReduction
  );
}

// Checks for an incoming emergency-stop command from the serial stream.
bool StepperController::emergencyStopRequested() {
  if (Serial.available() <= 0) {
    return false;
  }

  char incomingCharacter = (char)Serial.peek();

  if (incomingCharacter != 'x' && incomingCharacter != 'X') {
    return false;
  }

  Serial.read();

  while (Serial.available() > 0) {
    char leftover = (char)Serial.peek();

    if (leftover == '\r' || leftover == '\n') {
      Serial.read();
    }
    else {
      break;
    }
  }

  Serial.println();
  Serial.println(F("EMERGENCY STOP"));

  return true;
}

// Sends a signed pulse train to the selected motor using the supplied motion profile.
long StepperController::moveSteps(
  Config::MotorId motor,
  long signedSteps,
  bool positiveDirectionLevel,
  const Config::MotionProfile &profile,
  bool keepEnabledAfterMove
) {
  if (motor == Config::MotorId::None || signedSteps == 0) {
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

  // only cycle the selected axis while leaving the others alone
  disableMotor(motor);

  digitalWrite(stepPin, LOW);
  digitalWrite(directionPin, directionLevel ? HIGH : LOW);
  delayMicroseconds(Config::DIRECTION_SETUP_TIME_US);

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
    delayMicroseconds(Config::STEP_HIGH_TIME_US);

    digitalWrite(stepPin, LOW);
    delayMicroseconds(pulseDelay);

    completedSteps++;
  }

  digitalWrite(stepPin, LOW);

  if (!keepEnabledAfterMove) {
    disableMotor(motor);
  }

  if (movingPositive) {
    return (long)completedSteps;
  }

  return -(long)completedSteps;
}

// Prints the SPI connection test results for each TMC5160 driver.
void StepperController::printConnectionTests(Stream &output) {
  output.println();
  output.println(F("TMC5160 SPI connection tests:"));

  output.print(F("  Lead screw: "));
  output.println(leadDriver.test_connection());

  output.print(F("  Barrel:     "));
  output.println(barrelDriver.test_connection());

  output.print(F("  Yaw:        "));
  output.println(yawDriver.test_connection());

  output.println(F("A result of 0 normally indicates a successful connection."));
}
