#include "ServoController.h"

ServoController::ServoController()
  : leftAngle(Config::LEFT_SERVO_REST),
    rightAngle(Config::RIGHT_SERVO_REST)
{
}

// Attaches both servos and jumps them (not smoothed) to rest, twice, with a
// 300 ms wait after each write. Tracked angles are assumed, not read back.
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

// Immediate write of both angles (clamped to 0-180); no ramp, no wait.
void ServoController::writeServos(int left, int right)
{
  leftAngle = constrain(left, 0, 180);
  rightAngle = constrain(right, 0, 180);

  leftServo.write(leftAngle);
  rightServo.write(rightAngle);
}

// Moves angle one SERVO_ANGLE_STEP toward target without overshooting it.
static int stepToward(int angle, int target)
{
  if (angle < target)
  {
    return min(angle + Config::SERVO_ANGLE_STEP, target);
  }

  return max(angle - Config::SERVO_ANGLE_STEP, target);
}

// Blocking. Moves both servos together by SERVO_ANGLE_STEP per SERVO_STEP_DELAY_MS
// from the tracked angles to the (clamped) targets.
void ServoController::moveServosSmooth(
  int targetLeft,
  int targetRight
)
{
  targetLeft = constrain(targetLeft, 0, 180);
  targetRight = constrain(targetRight, 0, 180);

  const unsigned long stepDelay =
    Config::SERVO_STEP_DELAY_MS;

  while (
    leftAngle != targetLeft ||
    rightAngle != targetRight
  )
  {
    leftAngle = stepToward(leftAngle, targetLeft);
    rightAngle = stepToward(rightAngle, targetRight);

    leftServo.write(leftAngle);
    rightServo.write(rightAngle);

    delay(stepDelay);
  }
}

// Smooth move of both servos to the rest angles.
void ServoController::reset()
{
  moveServosSmooth(
    Config::LEFT_SERVO_REST,
    Config::RIGHT_SERVO_REST
  );
}

// Smooth move of both servos to the arm angles.
void ServoController::arm()
{
  moveServosSmooth(
    Config::LEFT_SERVO_ARM,
    Config::RIGHT_SERVO_ARM
  );
}

// Smooth move of both servos to the fire angles, hold SERVO_FIRE_HOLD_MS, then
// smooth return to rest.
void ServoController::fire()
{
  moveServosSmooth(
    Config::LEFT_SERVO_FIRE,
    Config::RIGHT_SERVO_FIRE
  );

  delay(Config::SERVO_FIRE_HOLD_MS);

  reset();
}

// Prints the tracked (last commanded) angles, not measured ones.
void ServoController::printStatus(Stream &output) const
{
  output.print(F("Left servo: "));
  output.print(leftAngle);

  output.print(F("  Right servo: "));
  output.println(rightAngle);
}