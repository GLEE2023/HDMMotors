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

// Configures the limit and barrel-presence switches.
void setupSwitches()
{
  pinMode(
    Config::BOTTOM_LIMIT_SWITCH_PIN,
    INPUT_PULLUP
  );

  pinMode(
    Config::TOP_BARREL_SWITCH_PIN,
    INPUT_PULLUP
  );
}

// Configures the burnwire trigger output.
void setupBurnwire()
{
  pinMode(Config::BURNWIRE_TRIGGER_PIN, OUTPUT);
  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
}

// Activates the burnwire for a short pulse using the transistor driver.
void fireBurnwireForDuration(unsigned long durationMs)
{
  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
}

// Returns true when a puck is detected at the firing position.
bool isPuckInBarrel()
{
  // Active-low switch.
  return digitalRead(
    Config::TOP_BARREL_SWITCH_PIN
  ) == LOW;
}

// Returns true when the elevator reaches the lower limit.
bool isElevatorAtBottom()
{
  return digitalRead(
    Config::BOTTOM_LIMIT_SWITCH_PIN
  ) == LOW;
}

// Skips separators before numeric command values.
const char *skipCommandSeparators(const char *text)
{
  while (
    *text == ' ' ||
    *text == '\t' ||
    *text == ':' ||
    *text == '='
  )
  {
    text++;
  }

  return text;
}

// Parses a signed integer from a command.
bool parseLongValue(const char *text, long &value)
{
  text = skipCommandSeparators(text);

  if (*text == '\0')
  {
    return false;
  }

  char *endPointer = nullptr;
  long parsedValue = strtol(
    text,
    &endPointer,
    10
  );

  if (endPointer == text)
  {
    return false;
  }

  while (
    *endPointer == ' ' ||
    *endPointer == '\t'
  )
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

// Parses a floating-point value from a command.
bool parseFloatValue(const char *text, float &value)
{
  text = skipCommandSeparators(text);

  if (*text == '\0')
  {
    return false;
  }

  char *endPointer = nullptr;
  double parsedValue = strtod(
    text,
    &endPointer
  );

  if (endPointer == text)
  {
    return false;
  }

  while (
    *endPointer == ' ' ||
    *endPointer == '\t'
  )
  {
    endPointer++;
  }

  if (*endPointer != '\0')
  {
    return false;
  }

  value = static_cast<float>(parsedValue);
  return true;
}

// Prints the available commands.
void printCommands()
{
  Serial.println();
  Serial.println(F("========== COMMANDS =========="));
  Serial.println(F("  F / f   Fire one puck if present"));
  Serial.println(F("  H / h   Home lead screw to bottom position"));
  Serial.println(F("  w       Reset servos"));
  Serial.println(F("  e       Arm servos"));
  Serial.println(F("  p       Fire right servo once"));
  Serial.println(F("  l       Fire left servo once"));
  Serial.println(F("  m / M   Fire both servos"));
  Serial.println(F("  W / S   Raise / Lower elevator 1 puck"));
  Serial.println(F("  + / -   Move lead screw up / down 1 mm"));
  Serial.println(F("  A / D / B   Move lead screw to full-up / bottom / full-down"));
  Serial.println(F("  L       Set current lead position as bottom"));
  Serial.println(F("  N / P   Move barrel to next / previous index"));
  Serial.println(F("  I5      Move barrel directly to index 5"));
  Serial.println(F("  O       Set current barrel position as index 0"));
  Serial.println(F("  Y12.5   Move absolute yaw angle"));
  Serial.println(F("  R0.25   Move relative yaw angle"));
  Serial.println(F("  Z       Set current yaw position as zero"));
  Serial.println(F("  C       Print system status"));
  Serial.println(F("  T / t   Test SPI connection for TMC5160 drivers"));
  Serial.println(F("  U       Pulse burnwire for 4 seconds"));
  Serial.println(F("  ?       Show this command list"));
  Serial.println(F("=============================="));
}

// Runs one complete fire sequence: detect a puck, fire it, advance the system, and return home.
void executeSinglePuckCycle() {
  Serial.println(F("--- STARTING SINGLE PUCK DEPLOYMENT ---"));

  if (!isPuckInBarrel())
  {
    Serial.println(
      F("No puck detected at the firing position.")
    );

    Serial.println(
      F("Skipping this deployment cycle.")
    );

    return;
  }

  int8_t currentPuckLevel = axes.getLeadPuckLevel();

  if (currentPuckLevel < 0) {
    currentPuckLevel = 0;
  }

  uint8_t targetPuckLevel = currentPuckLevel <= 0 ? 1 : (uint8_t)currentPuckLevel;

  if (targetPuckLevel > Config::PUCK_COUNT) {
    Serial.println(F("No additional puck level available. Returning elevator to bottom position..."));
    axes.moveLeadToBottom();
    return;
  }

  Serial.print(F("Positioning puck level "));
  Serial.println(targetPuckLevel);

  if (!axes.moveLeadToPuckLevel(targetPuckLevel)) {
    Serial.println(F("Unable to position the elevator for the current puck level."));
    return;
  }

  Serial.println(F("Arming servos..."));
  servoController.arm();

  Serial.println(F("Dropping lead screw 3 mm for launch..."));
  axes.moveLeadRelativeMillimeters(-3.0f);

  Serial.println(F("Launching puck..."));

  // Both servos fire during the automatic deployment.
  servoController.fire();

  Serial.println(F("Returning lead screw to normal height..."));
  axes.moveLeadRelativeMillimeters(3.0f);

  Serial.println(
    F("Raising lead screw for the next puck...")
  );

  bool raised = axes.moveLeadUpOnePuck();

  if (raised)
  {
    Serial.println(
      F("Advancing barrel to the next chamber...")
    );

    axes.moveBarrelToNextIndex();
  }
  else {
    Serial.println(F("No additional puck level available. Returning elevator to bottom position..."));
  }

  Serial.println(F("Returning elevator to bottom position..."));
  axes.moveLeadToBottom();
}

// Parses and executes one serial command.
void processCommand(const char *command)
{
  while (
    *command == ' ' ||
    *command == '\t'
  )
  {
    command++;
  }

  if (*command == '\0')
  {
    return;
  }

  char cmd = command[0];

  // ========================================================
  // COMPLETE DEPLOYMENT
  // ========================================================

  if (cmd == 'F' || cmd == 'f')
  {
    executeSinglePuckCycle();
    return;
  }

  // ========================================================
  // HOME / HELP
  // ========================================================

  if (cmd == 'H' || cmd == 'h')
  {
    axes.moveLeadToBottomFullDown();
    return;
  }

  if (cmd == '?')
  {
    printCommands();
    return;
  }

  // ========================================================
  // CASE-SENSITIVE SERVO COMMANDS
  // ========================================================

  // Lowercase w resets both servos.
  if (cmd == 'w')
  {
    servoController.reset();
    Serial.println(F("Both servos reset."));
    return;
  }

  // Lowercase e arms both servos.
  if (cmd == 'e')
  {
    servoController.arm();
    Serial.println(F("Both servos armed."));
    return;
  }

  // Lowercase p fires only the right servo.
  // Uppercase P remains barrel previous.
  if (cmd == 'p')
  {
    servoController.fireRight();
    Serial.println(F("Right servo fired."));
    return;
  }

  // Lowercase l fires only the left servo.
  if (cmd == 'l')
  {
    servoController.fireLeft();
    Serial.println(F("Left servo fired."));
    return;
  }

  // M or m fires both servos.
  if (cmd == 'M' || cmd == 'm')
  {
    servoController.fire();
    Serial.println(F("Both servos fired."));
    return;
  }

  // Test the TMC5160 SPI connections.
  if (cmd == 'T' || cmd == 't')
  {
    stepperController.printConnectionTests(Serial);
    return;
  }

  // Pulse the burnwire trigger for 4 seconds.
  if (cmd == 'U' || cmd == 'u')
  {
    Serial.println(F("Burnwire pulse started."));
    fireBurnwireForDuration(12000);
    Serial.println(F("Burnwire pulse complete."));
    return;
  }

  char commandLetter =
    static_cast<char>(
      toupper(
        static_cast<unsigned char>(cmd)
      )
    );

  const char *valueText = command + 1;

  switch (commandLetter) {
    case 'W': axes.moveLeadUpOnePuck(); break;
    case 'S': axes.moveLeadDownOnePuck(); break;
    case '+': axes.moveLeadUpOneMillimeter(); break;
    case '-': axes.moveLeadDownOneMillimeter(); break;
    case 'A': axes.moveLeadToTop(); break;
    case 'D': axes.moveLeadToBottom(); break;
    case 'B': axes.moveLeadToBottomFullDown(); break;
    case 'L': axes.setLeadPositionAsBottom(); break;
    case 'N': axes.moveBarrelToNextIndex(); break;
    case 'P': axes.moveBarrelToPreviousIndex(); break;

    case 'I':
    {
      long requestedIndex = 0;

      if (
        !parseLongValue(
          valueText,
          requestedIndex
        )
      )
      {
        Serial.println(
          F("Invalid index. Example: I5")
        );

        break;
      }

      axes.moveBarrelToIndex(
        static_cast<int>(requestedIndex)
      );

      break;
    }

    case 'O':
      axes.setBarrelPositionAsIndexZero();
      break;

    // ======================================================
    // YAW
    // ======================================================

    case 'Y':
    {
      float requestedAngle = 0.0f;

      if (
        !parseFloatValue(
          valueText,
          requestedAngle
        )
      )
      {
        Serial.println(
          F("Invalid yaw angle. Example: Y12.5")
        );

        break;
      }

      axes.moveYawToDegrees(requestedAngle);
      break;
    }

    case 'R':
    {
      float requestedMovement = 0.0f;

      if (
        !parseFloatValue(
          valueText,
          requestedMovement
        )
      )
      {
        Serial.println(
          F("Invalid relative yaw. Example: R0.25")
        );

        break;
      }

      axes.moveYawRelativeDegrees(
        requestedMovement
      );

      break;
    }

    case 'Z':
      axes.setYawPositionAsZero();
      break;

    // ======================================================
    // STATUS
    // ======================================================

    case 'C':
      axes.printStatus(Serial);
      servoController.printStatus(Serial);

      Serial.print(
        F("Puck in Barrel Switch: ")
      );

      Serial.println(
        isPuckInBarrel()
          ? F("TRIGGERED")
          : F("OPEN")
      );

      Serial.print(
        F("Elevator Bottom Switch: ")
      );

      Serial.println(
        isElevatorAtBottom()
          ? F("TRIGGERED")
          : F("OPEN")
      );

      break;

    // ======================================================
    // EMERGENCY STOP
    // ======================================================

    case 'X':
      stepperController.disableAllMotors();
      Serial.println(F("All stepper motors stopped."));
      break;

    default:
      Serial.println(F("Unknown command."));
      printCommands();
      break;
  }
}

// Reads serial bytes until a complete command line is received.
void readSerialCommands()
{
  while (Serial.available() > 0)
  {
    char incomingCharacter =
      static_cast<char>(Serial.read());

    // Ignore carriage return.
    if (incomingCharacter == '\r')
    {
      continue;
    }

    // Process the command at newline.
    if (incomingCharacter == '\n')
    {
      commandBuffer[commandLength] = '\0';

      if (commandLength > 0)
      {
        processCommand(commandBuffer);
      }

      commandLength = 0;
      continue;
    }

    if (
      commandLength <
      sizeof(commandBuffer) - 1
    )
    {
      commandBuffer[commandLength++] =
        incomingCharacter;
    }
  }
}

// Initializes serial, switches, stepper drivers, and servos.
void setup()
{
  Serial.begin(Config::SERIAL_BAUD);

  setupSwitches();
  stepperController.begin();
  servoController.begin();

  Serial.println(
    F("HDM Motor and Servo Controller Ready.")
  );

  printCommands();
}

// Continuously checks for new serial commands.
void loop()
{
  readSerialCommands();
}