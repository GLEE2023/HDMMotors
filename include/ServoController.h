#pragma once

#include <Arduino.h>
#include <Servo.h>

#include "Config.h"

// Controls the two mirrored firing servos used to push the puck.
class ServoController {
public:
  // Creates the controller with the configured rest positions.
  ServoController();

  // Attaches both servos and moves them to rest.
  void begin();

  // Returns both servos to rest.
  void reset();

  // Moves both servos to the armed position.
  void arm();

  // Fires both servos and returns them to rest.
  void fire();

  // Fires only the left servo and returns it to rest.
  void fireLeft();

  // Fires only the right servo and returns it to rest.
  void fireRight();

  // Prints the current commanded servo angles.
  void printStatus(Stream &output) const;

private:
  Servo leftServo;
  Servo rightServo;

  int leftAngle;
  int rightAngle;

  // Writes angles to both servos.
  void writeServos(int left, int right);

  // Moves both servos smoothly toward their requested targets.
  void moveServosSmooth(int targetLeft, int targetRight);
};