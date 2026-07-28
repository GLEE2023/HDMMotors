#include "ServoController.h"

ServoController::ServoController()
  : leftAngle(Config::LEFT_SERVO_REST),
    rightAngle(Config::RIGHT_SERVO_REST)
{}

// Initializes both servos and places them in their resting positions.
void ServoController::begin()
{
  leftServo.attach(Config::LEFT_SERVO_PIN);
  rightServo.attach(Config::RIGHT_SERVO_PIN);

  writeServos(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );

  delay(300);

  // Reapply the rest position after attachment settles.
  writeServos(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );

  delay(300);
}

// Applies the requested servo angles after clamping them to 0–180 degrees.
void ServoController::writeServos(int left, int right)
{
  leftAngle = constrain(left, 0, 180);
  rightAngle = constrain(right, 0, 180);

  leftServo.write(leftAngle);
  rightServo.write(rightAngle);
}

// Moves each servo in small increments until both reach their targets.
void ServoController::moveServosSmooth(
  int targetLeft,
  int targetRight
)
{
  const int stepSize = Config::SERVO_ANGLE_STEP;
  const unsigned long stepDelay =
    Config::SERVO_STEP_DELAY_MS;

  while (leftAngle != targetLeft || rightAngle != targetRight) {
    if (leftAngle < targetLeft) {
      leftAngle += stepSize;

      if (leftAngle > targetLeft) {
        leftAngle = targetLeft;
      }
    }
    else if (leftAngle > targetLeft) {
      leftAngle -= stepSize;

      if (leftAngle < targetLeft) {
        leftAngle = targetLeft;
      }
    }

    // Move right servo toward its target.
    if (rightAngle < targetRight) {
      rightAngle += stepSize;

      if (rightAngle > targetRight) {
        rightAngle = targetRight;
      }
    }
    else if (rightAngle > targetRight) {
      rightAngle -= stepSize;

      if (rightAngle < targetRight) {
        rightAngle = targetRight;
      }
    }

    // Send both commands on every movement step.
    leftServo.write(leftAngle);
    rightServo.write(rightAngle);

    delay(stepDelay);
  }
}

// Returns both servos to their resting positions.
void ServoController::reset()
{
  moveServosSmooth(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );
}

// Feeds/arms both servos.
// Pressing e in main.cpp calls this function.
void ServoController::arm()
{
  moveServosSmooth(
    Config::LEFT_SERVO_ARM,
    Config::RIGHT_SERVO_ARM
  );
}

// Fires both servos from their current armed position,
// then returns both servos to rest.
void ServoController::fire()
{
  moveServosSmooth(
    Config::LEFT_SERVO_FIRE,
    Config::RIGHT_SERVO_FIRE
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  moveServosSmooth(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );
}

// Fires only the left servo from the armed position.
// The right servo remains armed during the firing movement.
// After firing, both servos return to rest.
void ServoController::fireLeft()
{
  const int stationaryRightAngle = rightAngle;

  moveServosSmooth(
    Config::LEFT_SERVO_FIRE,
    stationaryRightAngle
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  moveServosSmooth(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );
}

// Fires only the right servo from the armed position.
// The left servo remains armed during the firing movement.
// After firing, both servos return to rest.
void ServoController::fireRight()
{
  const int stationaryLeftAngle = leftAngle;

  moveServosSmooth(
    stationaryLeftAngle,
    Config::RIGHT_SERVO_FIRE
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  moveServosSmooth(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );
}

// Prints the current servo angles.
void ServoController::printStatus(Stream &output) const
{
  output.print(F("Left servo: "));
  output.print(leftAngle);

  output.print(F("  Right servo: "));
  output.println(rightAngle);
}