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

  Serial.println(
    F("  F / f   Run complete single-puck deployment")
  );

  Serial.println(
    F("  H / h   Print this command menu")
  );

  Serial.println();
  Serial.println(F("  SERVO COMMANDS"));

  Serial.println(
    F("  w       Reset both servos")
  );

  Serial.println(
    F("  e       Arm both servos")
  );

  Serial.println(
    F("  p       Fire RIGHT servo only")
  );

  Serial.println(
    F("  L / l   Fire LEFT servo only")
  );

  Serial.println(
    F("  M / m   Fire BOTH servos")
  );

  Serial.println();
  Serial.println(F("  LEAD-SCREW COMMANDS"));

  Serial.println(
    F("  W / S   Raise / lower elevator one puck")
  );

  Serial.println(
    F("  A / D   Move lead screw to top / bottom")
  );

  Serial.println(
    F("  B       Set current lead position as bottom")
  );

  Serial.println();
  Serial.println(F("  BARREL COMMANDS"));

  Serial.println(
    F("  N / P   Move barrel to next / previous index")
  );

  Serial.println(
    F("  I5      Move barrel directly to index 5")
  );

  Serial.println(
    F("  O       Set current barrel position as index 0")
  );

  Serial.println();
  Serial.println(F("  YAW COMMANDS"));

  Serial.println(
    F("  Y12.5   Move to an absolute yaw angle")
  );

  Serial.println(
    F("  R0.25   Move by a relative yaw angle")
  );

  Serial.println(
    F("  Z       Set current yaw position as zero")
  );

  Serial.println();
  Serial.println(F("  SYSTEM COMMANDS"));

  Serial.println(
    F("  C       Print system status")
  );

  Serial.println(
    F("  T / t   Test TMC5160 SPI connections")
  );

  Serial.println(
    F("  X       Disable all stepper motors")
  );

  Serial.println(F("=============================="));
}

// Runs the complete deployment sequence.
void executeSinglePuckCycle()
{
  Serial.println(
    F("--- STARTING SINGLE PUCK DEPLOYMENT ---")
  );

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

  Serial.println(F("Launching puck..."));

  // Both servos fire during the automatic deployment.
  servoController.fire();

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
  else
  {
    Serial.println(
      F("No additional puck level is available.")
    );
  }

  Serial.println(
    F("Returning elevator to bottom position...")
  );

  axes.moveLeadToBottom();

  Serial.println(
    F("--- DEPLOYMENT CYCLE COMPLETE ---")
  );
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
  // HELP
  // ========================================================

  if (
    cmd == 'H' ||
    cmd == 'h' ||
    cmd == '?'
  )
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

  // L or l fires only the left servo.
  if (cmd == 'L' || cmd == 'l')
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

  char commandLetter =
    static_cast<char>(
      toupper(
        static_cast<unsigned char>(cmd)
      )
    );

  const char *valueText = command + 1;

  switch (commandLetter)
  {
    // ======================================================
    // LEAD SCREW
    // ======================================================

    case 'W':
      axes.moveLeadUpOnePuck();
      break;

    case 'S':
      axes.moveLeadDownOnePuck();
      break;

    case 'A':
      axes.moveLeadToTop();
      break;

    case 'D':
      axes.moveLeadToBottom();
      break;

    // Changed from L to B because L is now left servo.
    case 'B':
      axes.setLeadPositionAsBottom();
      Serial.println(
        F("Current lead position set as bottom.")
      );
      break;

    // ======================================================
    // BARREL
    // ======================================================

    case 'N':
      axes.moveBarrelToNextIndex();
      break;

    // Only uppercase P reaches this case because lowercase p
    // was already handled as the right-servo command.
    case 'P':
      axes.moveBarrelToPreviousIndex();
      break;

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