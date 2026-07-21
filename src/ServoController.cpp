#include "ServoController.h"

ServoController::ServoController()
  : leftAngle(Config::LEFT_SERVO_REST),
    rightAngle(Config::RIGHT_SERVO_REST)
{}

// Initializes both servos and places them into their resting state on startup.
void ServoController::begin() {
  leftServo.attach(Config::LEFT_SERVO_PIN);
  rightServo.attach(Config::RIGHT_SERVO_PIN);

  writeServos(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );

  delay(200);
}

// Applies the requested servo angles after clamping them to the safe 0-180 degree range.
void ServoController::writeServos(int left, int right) {
  leftAngle = constrain(left, 0, 180);
  rightAngle = constrain(right, 0, 180);

  leftServo.write(leftAngle);
  rightServo.write(rightAngle);
}

// Moves each servo in small increments so the motion looks smoother than a single jump.
void ServoController::moveServosSmooth(int targetLeft, int targetRight) {
  const int step = Config::SERVO_ANGLE_STEP;
  const unsigned long wait = Config::SERVO_STEP_DELAY_MS;

  while (leftAngle != targetLeft || rightAngle != targetRight) {
    if (leftAngle < targetLeft) leftAngle += step;
    else if (leftAngle > targetLeft) leftAngle -= step;

    if (rightAngle < targetRight) rightAngle += step;
    else if (rightAngle > targetRight) rightAngle -= step;

    if (abs(leftAngle - targetLeft) < step) leftAngle = targetLeft;
    if (abs(rightAngle - targetRight) < step) rightAngle = targetRight;

    writeServos(leftAngle, rightAngle);
    delay(wait);
  }
}

// Returns the servos to their resting angles.
void ServoController::reset() {
  moveServosSmooth(Config::LEFT_SERVO_REST, Config::RIGHT_SERVO_REST);
}

// Moves the servos into the armed position before a firing command.
void ServoController::arm() {
  moveServosSmooth(Config::LEFT_SERVO_ARM, Config::RIGHT_SERVO_ARM);
}

// Executes a fast fire pulse and then eases the servos back to rest.
void ServoController::fire() {
  // Explosive Pinball Push into Flywheels (Bypasses slow interpolation)
  writeServos(Config::LEFT_SERVO_FIRE, Config::RIGHT_SERVO_FIRE);

  delay(Config::SERVO_FIRE_HOLD_MS);

  // Controlled return to rest position
  moveServosSmooth(Config::LEFT_SERVO_REST, Config::RIGHT_SERVO_REST);
}

// Prints the current servo angles for debugging and operator feedback.
void ServoController::printStatus(Stream &output) const {
  output.print(F("Left servo: "));
  output.print(leftAngle);
  output.print(F("  Right servo: "));
  output.println(rightAngle);
}