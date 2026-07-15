#include <Servo.h>

Servo myServo;

const int servoPin = 3;
const int servoStepDelayMilliseconds = 10;

int currentAngle = 0;

void moveServoSmooth(int targetAngle)
{
  targetAngle = constrain(targetAngle, 0, 180);

  const int angleStep = 5;
  const int movementDelay = 5;

  if (targetAngle > currentAngle)
  {
    for (int position = currentAngle; position < targetAngle; position += angleStep)
    {
      myServo.write(position);
      delay(movementDelay);
    }
  }
  else
  {
    for (int position = currentAngle; position > targetAngle; position -= angleStep)
    {
      myServo.write(position);
      delay(movementDelay);
    }
  }

  myServo.write(targetAngle);
  currentAngle = targetAngle;
}

void printControls()
{
  Serial.println();
  Serial.println("===== Arduino Nano Servo Controller =====");
  Serial.println("w = reset");
  Serial.println("e = arm");
  Serial.println("p = fire");
  Serial.println("=========================================");
}

void setup()
{
  Serial.begin(115200);

  myServo.attach(servoPin, 500, 2400);

  currentAngle = 0;
  myServo.write(currentAngle);

  delay(500);

  printControls();
}

void loop()
{
  if (Serial.available() == 0)
  {
    return;
  }

  char command = Serial.read();
  bool commandHandled = true;

  switch (command)
  {
    case 'w':
      moveServoSmooth(currentAngle + 180);
      break;

    case 'e':
      moveServoSmooth(currentAngle - 45);
      break;

    case 'p':
      moveServoSmooth(currentAngle - 90);
      delay(100);
      moveServoSmooth(currentAngle + 180);
      break;

    case '?':
      printControls();
      break;

    case 'h':
      printControls();
      break;

    case '\r':
    case '\n':
      commandHandled = false;
      break;

    default:
      commandHandled = false;
      Serial.print("Unknown command: ");
      Serial.println(command);
      break;
  }

  if (commandHandled)
  {
    Serial.print("Current angle: ");
    Serial.print(currentAngle);
    Serial.println(" degrees");
  }
}
