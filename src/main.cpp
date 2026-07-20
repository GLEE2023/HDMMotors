#include <Arduino.h>
#include <stdlib.h>
#include <ctype.h>

#include "Axes.h"
#include "Config.h"
#include "ServoController.h"
#include "StepperController.h"

StepperController stepperController;
Axes axes(stepperController);
ServoController servoController;

char commandBuffer[32];
uint8_t commandLength = 0;

void setupSwitches()
{
  pinMode(Config::BOTTOM_LIMIT_SWITCH_PIN, INPUT_PULLUP);
  pinMode(Config::TOP_BARREL_SWITCH_PIN, INPUT_PULLUP);
}

bool isPuckInBarrel()
{
  // Assumes ACTIVE LOW switch setup (grounded when closed/triggered)
  return digitalRead(Config::TOP_BARREL_SWITCH_PIN) == LOW;
}

bool isElevatorAtBottom()
{
  return digitalRead(Config::BOTTOM_LIMIT_SWITCH_PIN) == LOW;
}

const char *skipCommandSeparators(const char *text)
{
  while (*text == ' ' || *text == '\t' || *text == ':' || *text == '=')
  {
    text++;
  }

  return text;
}

bool parseLongValue(const char *text, long &value)
{
  text = skipCommandSeparators(text);

  if (*text == '\0')
  {
    return false;
  }

  char *endPointer = nullptr;
  long parsedValue = strtol(text, &endPointer, 10);

  if (endPointer == text)
  {
    return false;
  }

  while (*endPointer == ' ' || *endPointer == '\t')
  {
    endPointer++;
  }

  if (*endPointer != '\0')
  {
    return false;
  }

  value = parsedValue;
  return true;
}

bool parseFloatValue(const char *text, float &value)
{
  text = skipCommandSeparators(text);

  if (*text == '\0')
  {
    return false;
  }

  char *endPointer = nullptr;
  double parsedValue = strtod(text, &endPointer);

  if (endPointer == text)
  {
    return false;
  }

  while (*endPointer == ' ' || *endPointer == '\t')
  {
    endPointer++;
  }

  if (*endPointer != '\0')
  {
    return false;
  }

  value = (float)parsedValue;
  return true;
}

void printCommands()
{
  Serial.println();
  Serial.println(F("========== COMMANDS =========="));
  Serial.println(F("  F / f   Fire one puck if present"));
  Serial.println(F("  H / h   Home lead screw to bottom position"));
  Serial.println(F("  w       Reset servos"));
  Serial.println(F("  e       Arm servos"));
  Serial.println(F("  p       Raw servo fire pulse"));
  Serial.println(F("  W / S   Raise / Lower elevator 1 puck"));
  Serial.println(F("  A / D   Move lead screw to full-up / bottom"));
  Serial.println(F("  L       Set current lead position as bottom"));
  Serial.println(F("  N / P   Move barrel to next / previous index"));
  Serial.println(F("  I5      Move barrel directly to index 5"));
  Serial.println(F("  O       Set current barrel position as index 0"));
  Serial.println(F("  Y12.5   Move absolute yaw angle"));
  Serial.println(F("  R0.25   Move relative yaw angle"));
  Serial.println(F("  Z       Set current yaw position as zero"));
  Serial.println(F("  C       Print system status"));
  Serial.println(F("=============================="));
}

void executeSinglePuckCycle()
{
  Serial.println(F("--- STARTING SINGLE PUCK DEPLOYMENT ---"));

  if (!isPuckInBarrel())
  {
    Serial.println(F("No puck detected at the firing position. Skipping this cycle."));
    return;
  }

  Serial.println(F("Launching puck..."));
  servoController.fire();

  Serial.println(F("Raising lead screw for the next puck..."));
  bool raised = axes.moveLeadUpOnePuck();

  if (raised)
  {
    Serial.println(F("Advancing barrel to the next chamber..."));
    axes.moveBarrelToNextIndex();
  }
  else
  {
    Serial.println(F("No additional puck level available. Returning elevator to bottom position..."));
  }

  Serial.println(F("Returning elevator to bottom position..."));
  axes.moveLeadToBottom();
}

void processCommand(const char *command)
{
  while (*command == ' ' || *command == '\t')
  {
    command++;
  }

  if (*command == '\0')
  {
    return;
  }

  char cmd = command[0];

  if (cmd == 'F' || cmd == 'f')
  {
    executeSinglePuckCycle();
    return;
  }

  if (cmd == 'H' || cmd == 'h' || cmd == '?')
  {
    printCommands();
    return;
  }

  if (cmd == 'w') { servoController.reset(); return; }
  if (cmd == 'e') { servoController.arm(); return; }
  if (cmd == 'p') { servoController.fire(); return; }

  char commandLetter = (char)toupper((unsigned char)cmd);
  const char *valueText = command + 1;

  switch (commandLetter)
  {
    case 'W': axes.moveLeadUpOnePuck(); break;
    case 'S': axes.moveLeadDownOnePuck(); break;
    case 'A': axes.moveLeadToTop(); break;
    case 'D': axes.moveLeadToBottom(); break;
    case 'L': axes.setLeadPositionAsBottom(); break;
    case 'N': axes.moveBarrelToNextIndex(); break;
    case 'P': axes.moveBarrelToPreviousIndex(); break;

    case 'I':
    {
      long requestedIndex = 0;
      if (!parseLongValue(valueText, requestedIndex))
      {
        Serial.println(F("Invalid index. Example: I5"));
        break;
      }
      axes.moveBarrelToIndex((int)requestedIndex);
      break;
    }

    case 'O': axes.setBarrelPositionAsIndexZero(); break;

    case 'Y':
    {
      float requestedAngle = 0.0f;
      if (!parseFloatValue(valueText, requestedAngle))
      {
        Serial.println(F("Invalid yaw angle. Example: Y12.5"));
        break;
      }
      axes.moveYawToDegrees(requestedAngle);
      break;
    }

    case 'R':
    {
      float requestedMovement = 0.0f;
      if (!parseFloatValue(valueText, requestedMovement))
      {
        Serial.println(F("Invalid relative yaw. Example: R0.25"));
        break;
      }
      axes.moveYawRelativeDegrees(requestedMovement);
      break;
    }

    case 'Z': axes.setYawPositionAsZero(); break;

    case 'C':
      axes.printStatus(Serial);
      servoController.printStatus(Serial);
      Serial.print(F("Puck in Barrel Switch: "));
      Serial.println(isPuckInBarrel() ? F("TRIGGERED") : F("OPEN"));
      Serial.print(F("Elevator Bottom Switch: "));
      Serial.println(isElevatorAtBottom() ? F("TRIGGERED") : F("OPEN"));
      break;

    case 'X':
      stepperController.disableAllMotors();
      Serial.println(F("All motors stopped."));
      break;

    default:
      Serial.println(F("Unknown command."));
      printCommands();
      break;
  }
}

void readSerialCommands()
{
  while (Serial.available() > 0)
  {
    char incomingCharacter = (char)Serial.read();

    if (incomingCharacter == '\r') continue;

    if (incomingCharacter == '\n')
    {
      commandBuffer[commandLength] = '\0';
      if (commandLength > 0) processCommand(commandBuffer);
      commandLength = 0;
      continue;
    }

    if (commandLength < sizeof(commandBuffer) - 1)
    {
      commandBuffer[commandLength++] = incomingCharacter;
    }
  }
}

void setup()
{
  Serial.begin(Config::SERIAL_BAUD);

  setupSwitches();
  stepperController.begin();
  servoController.begin();

  Serial.println(F("Single-Button Fire Sequence Ready."));
  printCommands();
}

void loop()
{
  readSerialCommands();
}