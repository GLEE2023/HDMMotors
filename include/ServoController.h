#pragma once

#include <Arduino.h>
#include <Servo.h>

#include "Config.h"

// Drives the two firing servos with a simple stateful interface.
// Controls the firing servos used to push the puck out of the barrel.
class ServoController {
public:
  // Creates a servo controller with the initial rest positions loaded.
  ServoController();

  // Initializes the servos and puts them in their rest position.
  void begin();
  // Returns the servos to their resting angles.
  void reset();
  // Moves the servos into the armed position.
  void arm();
  // Executes one firing pulse and then returns to rest.
  void fire();
  // Prints the current servo angles for debugging.
  void printStatus(Stream &output) const;

private:
  Servo leftServo;
  Servo rightServo;

  int leftAngle;
  int rightAngle;

  // Writes a specific angle to both servos after clamping it to the valid range.
  void writeServos(int left, int right);
  // Moves each servo in small steps so the motion looks smoother.
  void moveServosSmooth(int targetLeft, int targetRight);
};
