#include "ServoController.h"

ServoController::ServoController()
  : leftAngle(Config::LEFT_SERVO_REST),
    rightAngle(Config::RIGHT_SERVO_REST)
{
}

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
  targetLeft = constrain(targetLeft, 0, 180);
  targetRight = constrain(targetRight, 0, 180);

  const int stepSize = Config::SERVO_ANGLE_STEP;
  const unsigned long stepDelay =
    Config::SERVO_STEP_DELAY_MS;

  while (
    leftAngle != targetLeft ||
    rightAngle != targetRight
  )
  {
    // Move left servo toward its target.
    if (leftAngle < targetLeft)
    {
      leftAngle += stepSize;

      if (leftAngle > targetLeft)
      {
        leftAngle = targetLeft;
      }
    }
    else if (leftAngle > targetLeft)
    {
      leftAngle -= stepSize;

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
void ServoController::arm()
{
  moveServosSmooth(
    Config::LEFT_SERVO_ARM,
    Config::RIGHT_SERVO_ARM
  );
}

// Fires both servos and returns them to rest.
void ServoController::fire()
{
  moveServosSmooth(
    Config::LEFT_SERVO_FIRE,
    Config::RIGHT_SERVO_FIRE
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  reset();
}

// Fires only the left servo.
// The right servo stays at its current position during firing.
void ServoController::fireLeft()
{
  const int stationaryRightAngle = rightAngle;

  moveServosSmooth(
    Config::LEFT_SERVO_FIRE,
    stationaryRightAngle
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  reset();
}

// Fires only the right servo.
// The left servo stays at its current position during firing.
void ServoController::fireRight()
{
  const int stationaryLeftAngle = leftAngle;

  moveServosSmooth(
    stationaryLeftAngle,
    Config::RIGHT_SERVO_FIRE
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  reset();
}

// Prints the current servo angles.
void ServoController::printStatus(Stream &output) const
{
  output.print(F("Left servo: "));
  output.print(leftAngle);

  output.print(F("  Right servo: "));
  output.println(rightAngle);
}