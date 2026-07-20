#pragma once

#include <Arduino.h>
#include <Servo.h>

#include "Config.h"

class ServoController
{
public:
  ServoController();

  void begin();
  void reset();
  void arm();
  void fire();
  void printStatus(Stream &output) const;

private:
  Servo leftServo;
  Servo rightServo;

  int leftAngle;
  int rightAngle;

  void writeServos(int left, int right);
  void moveServosSmooth(int targetLeft, int targetRight);
};
