#include <Arduino.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "Axes.h"
#include "Config.h"
#include "Sensors.h"
#include "ServoController.h"
#include "StepperController.h"

Sensors sensors;
StepperController stepperController(sensors);
Axes axes(stepperController, sensors);
ServoController servoController;

// One serial line being assembled (31 characters plus terminator).
// commandOverflowed marks a line that exceeded the buffer; it is dropped at newline.
char commandBuffer[32];
uint8_t commandLength = 0;
bool commandOverflowed = false;

// Skips spaces, tabs, ':' and '=' between a command letter and its number.
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

// True if nothing but spaces/tabs remains in text.
bool onlyBlanksRemain(const char *text)
{
  while (*text == ' ' || *text == '\t')
  {
    text++;
  }

  return *text == '\0';
}

// Parses a base-10 integer that must be the whole remaining text (trailing
// spaces/tabs allowed). Returns false, leaving value unchanged, if the text is
// empty, not a number, out of range, or has trailing characters.
bool parseLongValue(const char *text, long &value)
{
  text = skipCommandSeparators(text);

  char *endPointer = nullptr;
  errno = 0;
  const long parsedValue = strtol(text, &endPointer, 10);

  if (endPointer == text || errno == ERANGE || !onlyBlanksRemain(endPointer))
  {
    return false;
  }

  value = parsedValue;
  return true;
}

// Same rules as parseLongValue, for floats. Also rejects NaN/infinity and
// values that overflow a float.
bool parseFloatValue(const char *text, float &value)
{
  text = skipCommandSeparators(text);

  char *endPointer = nullptr;
  errno = 0;
  const double parsedValue = strtod(text, &endPointer);

  if (
    endPointer == text ||
    errno == ERANGE ||
    !isfinite(parsedValue) ||
    !onlyBlanksRemain(endPointer)
  )
  {
    return false;
  }

  const float parsedFloat = static_cast<float>(parsedValue);

  if (!isfinite(parsedFloat))
  {
    return false;
  }

  value = parsedFloat;
  return true;
}

// Prints a fixed command list. It is static text, so it must be updated by hand
// when commands change. Case rules are not shown: only 'w' and 'e' are
// lowercase-only; most other letters also accept the opposite case.
void printCommands()
{
  Serial.println();
  Serial.println(F("========== COMMANDS =========="));
  Serial.println(F("  G / g   Run full deployment (raises pucks, Enter to fire each)"));
  Serial.println(F("  H / h   Return lead screw to configured bottom"));
  Serial.println(F("  w       Reset servos"));
  Serial.println(F("  e       Arm servos"));
  Serial.println(F("  m / M   Fire both servos"));
  Serial.println(F("  W / S   Raise / Lower elevator 1 puck"));
  Serial.println(F("  + / -   Move lead screw up / down 1 mm"));
  Serial.println(F("  N / P   Move barrel to next / previous index"));
  Serial.println(F("  B       Drive lead screw down for the full travel"));
  Serial.println(F("  I5      Move barrel directly to index 5"));
  Serial.println(F("  Y12.5   Move absolute yaw angle"));
  Serial.println(F("  R0.25   Move relative yaw angle"));
  Serial.println(F("  Z       Set current yaw position as zero"));
  Serial.println(F("  C       Print system status"));
  Serial.println(F("  T / t   Test SPI connection for TMC5160 drivers"));
  Serial.println(F("  X       Stop step pulses; disable configured drivers"));
  Serial.println(F("  U       Pulse burnwire for 12 seconds"));
  Serial.println(F("  ?       Show this command list"));
  Serial.println(F("=============================="));
}

// Used by H and by full deployment.
// - Bottom switch configured: runs moveLeadToBottomFullDown(), i.e. the same
//   full-travel descent as B, stopping early on the switch.
// - No bottom switch: moves to the tracked position 0 only (no extra travel).
void returnLeadToBottom()
{
  if (sensors.hasBottomSwitch())
  {
    axes.moveLeadToBottomFullDown();
  }
  else
  {
    Serial.println(F("Bottom limit switch is not configured; using the software bottom reference."));
    axes.moveLeadToBottom();
  }
}

// Blocks (busy-wait, no timeout) until a full line is received.
// Returns true for an empty line or a first non-blank character of Y/y;
// any other first character returns false. The prompt mentions "N", but any
// non-Y character cancels. Motors are idle here, so 'x' has no special effect.
bool waitForFireConfirmation()
{
  Serial.println(F("Press Enter to fire, or type N to cancel."));

  bool hasResponse = false;
  bool confirmed = true;

  while (true)
  {
    if (Serial.available() == 0)
    {
      continue;
    }

    char responseCharacter =
      static_cast<char>(Serial.read());

    if (responseCharacter == '\r')
    {
      continue;
    }

    if (responseCharacter == '\n')
    {
      return !hasResponse || confirmed;
    }

    if (!hasResponse && responseCharacter != ' ' && responseCharacter != '\t')
    {
      hasResponse = true;
      confirmed =
        responseCharacter == 'Y' ||
        responseCharacter == 'y';
    }
  }
}

// G command. From the bottom, raise to puck level 1, then per puck 1..PUCK_COUNT:
//   raise +PUCK_HEIGHT_MM -> arm servos -> check puck latch -> lower PRE_FIRE mm
//   -> wait for operator confirmation -> fire -> raise POST_FIRE mm.
// The next puck's +PUCK_HEIGHT_MM raise starts from the realigned position.
// A failed lead move stops the whole run (servos reset in most cases).
// With a puck switch, a level with no latched puck is skipped, not fired.
// Without a puck switch, a puck is assumed present at every level.
void executeFullDeployment() {
  // Start from the bottom; abort unless the lead level is known to be 0.
  returnLeadToBottom();

  if (axes.getLeadPuckLevel() != 0) {
    Serial.println(F("Unable to return the elevator to its bottom reference; deployment cancelled."));
    return;
  }

  if (!sensors.hasPuckSwitch()) {
    Serial.println(F("Puck switch is not configured; puck presence will not be verified."));
  }

  for (uint8_t puckNumber = 1; puckNumber <= Config::PUCK_COUNT; ++puckNumber) {
    // Forget any puck seen on the previous level before raising the next.
    sensors.clearPuckLatch();

    // First puck: raise to the bottom puck level, then one more puck height
    // into the deployment position. Later pucks: one puck height from the
    // realigned position left by the previous cycle.
    if (puckNumber == 1 && !axes.moveLeadToPuckLevel(1)) {
      Serial.println(F("Unable to raise to the first puck level; full deployment stopped."));
      servoController.reset();
      return;
    }

    if (!axes.moveLeadRelativeMillimeters(Config::PUCK_HEIGHT_MM)) {
      Serial.println(F("Unable to raise the puck to firing height; full deployment stopped."));
      servoController.reset();
      return;
    }

    servoController.arm();

    // The latch is set if the switch tripped at any point while raising (it is
    // polled during lead moves). Without the latch, skip firing and continue
    // to the next puck.
    sensors.updatePuckLatch();

    if (sensors.hasPuckSwitch() && !sensors.isPuckLatched()) {
      Serial.print(F("No puck detected for level "));
      Serial.print(puckNumber);
      Serial.println(F("; skipping deployment."));
      servoController.reset();
      continue;
    }

    // Lower the elevator slightly (servos stay armed) before firing.
    if (!axes.moveLeadRelativeMillimeters(
          -Config::FULL_DEPLOYMENT_PRE_FIRE_LOWER_MM
        )) {
      Serial.println(F("Unable to position the puck for firing; full deployment stopped."));
      servoController.reset();
      return;
    }

    // Cancel: servos to rest, elevator back to the bottom, run ends.
    if (!waitForFireConfirmation())
    {
      servoController.reset();
      returnLeadToBottom();
      Serial.println(F("Deployment cancelled."));
      return;
    }

    Serial.println(F("Firing servos..."));
    servoController.fire();
    sensors.clearPuckLatch();

    // Servos are already back at rest after fire(); this failure path does
    // not reset them again.
    if (!axes.moveLeadRelativeMillimeters(
          Config::FULL_DEPLOYMENT_POST_FIRE_RAISE_MM
        )) {
      Serial.println(F("Unable to finish the puck cycle; full deployment stopped."));
      return;
    }
  }

  // After the last puck, return to the bottom and report if that failed.
  returnLeadToBottom();

  if (axes.getLeadPuckLevel() != 0) {
    Serial.println(F("Full deployment finished, but the elevator did not reach the bottom reference."));
  }
}

// Runs one command line. The first character selects the command; the rest is
// its argument. Servo, T and U commands are matched first on the raw character
// (so 'w' and 'e' are lowercase-only); remaining commands are matched after
// uppercasing, so e.g. 's', 'b', 'n', 'p', 'i', 'y', 'r', 'z', 'c', 'x' work.
// Commands block until finished.
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

  if (cmd == 'G' || cmd == 'g')
  {
    executeFullDeployment();
    return;
  }

  // ========================================================
  // HOME / HELP
  // ========================================================

  // H: see returnLeadToBottom() (identical to B when a bottom switch is configured).
  if (cmd == 'H' || cmd == 'h')
  {
    returnLeadToBottom();
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

  // Lowercase w moves both servos to rest (uppercase W raises the elevator below).
  if (cmd == 'w')
  {
    servoController.reset();
    Serial.println(F("Both servos reset."));
    return;
  }

  // Lowercase e moves both servos to the armed position (uppercase E is unknown).
  if (cmd == 'e')
  {
    servoController.arm();
    Serial.println(F("Both servos armed."));
    return;
  }

  // M/m moves both servos to fire, holds, returns them to rest, and clears the
  // puck latch. Does not move the elevator or check for a puck.
  if (cmd == 'M' || cmd == 'm')
  {
    servoController.fire();
    sensors.clearPuckLatch();
    Serial.println(F("Both servos fired."));
    return;
  }

  // Prints the lead TMC5160 SPI test result only; barrel/yaw have no test.
  if (cmd == 'T' || cmd == 't')
  {
    stepperController.printConnectionTests(Serial);
    return;
  }

  // Drives the burnwire pin high for 12 s (blocking, no interrupt possible).
  // If the pin is unconfigured, nothing is driven and a message says so.
  if (cmd == 'U' || cmd == 'u')
  {
    if (!sensors.hasBurnwireTrigger())
    {
      Serial.println(F("Burnwire trigger pin is not configured; no pulse sent."));
      return;
    }

    Serial.println(F("Burnwire pulse started."));
    sensors.fireBurnwire(12000);
    Serial.println(F("Burnwire pulse complete."));
    return;
  }

  char commandLetter =
    static_cast<char>(
      toupper(
        static_cast<unsigned char>(cmd)
      )
    );

  // Text after the command letter, parsed as the numeric argument by I, Y, R.
  const char *valueText = command + 1;

  switch (commandLetter) {
    // Lead screw: one puck level up/down, 1 mm jogs, and B = full-travel
    // descent that ignores the tracked position (stops early on bottom switch).
    case 'W': axes.moveLeadUpOnePuck(); break;
    case 'S': axes.moveLeadDownOnePuck(); break;
    case '+': axes.moveLeadUpOneMillimeter(); break;
    case '-': axes.moveLeadDownOneMillimeter(); break;
    case 'B': axes.moveLeadToBottomFullDown(); break;
    // Barrel: next/previous index, wrapping around.
    case 'N': axes.moveBarrelToNextIndex(); break;
    case 'P': axes.moveBarrelToPreviousIndex(); break;

    // I<n>: barrel to index n; rejects unparsable or out-of-range n.
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

      if (
        requestedIndex < 0 ||
        requestedIndex >= Config::BARREL_POSITION_COUNT
      )
      {
        Serial.print(F("Barrel index must be between 0 and "));
        Serial.print(Config::BARREL_POSITION_COUNT - 1);
        Serial.println(F("."));
        break;
      }

      axes.moveBarrelToIndex(
        static_cast<int>(requestedIndex)
      );

      break;
    }

    // ======================================================
    // YAW
    // ======================================================

    // Y<deg>: absolute yaw angle (clamped to the yaw limits by Axes).
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

    // R<deg>: yaw relative to the tracked angle (result clamped to the limits).
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

    // Z: redefines the current yaw position as 0 without moving.
    case 'Z':
      axes.setYawPositionAsZero();
      break;

    // ======================================================
    // STATUS
    // ======================================================

    case 'C':
      axes.printStatus(Serial);
      servoController.printStatus(Serial);

      sensors.printStatus(Serial);

      break;

    // ======================================================
    // STOP / DISABLE
    // ======================================================

    // X as a typed command only runs between moves: it sets STEP pins low and
    // drives any configured enable pins to disable. Stopping a move already in
    // progress is handled inside StepperController (see emergencyStopRequested).
    case 'X':
      if (stepperController.disableAllMotors())
      {
        Serial.println(F("All stepper drivers disabled."));
      }
      else
      {
        Serial.println(F("All step pulses stopped; holding torque may remain on because an enable pin is not configured."));
      }
      break;

    default:
      Serial.println(F("Unknown command."));
      printCommands();
      break;
  }
}

// Drains available serial bytes into commandBuffer. At newline the line is run
// (empty lines ignored) or, if it was too long, discarded with a message.
// '\r' is ignored. Any command runs to completion before reading continues.
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
      if (commandOverflowed)
      {
        Serial.println(F("Command too long; discarded."));
      }
      else if (commandLength > 0)
      {
        commandBuffer[commandLength] = '\0';
        processCommand(commandBuffer);
      }

      commandLength = 0;
      commandOverflowed = false;
      continue;
    }

    if (!commandOverflowed && commandLength < sizeof(commandBuffer) - 1)
    {
      commandBuffer[commandLength++] =
        incomingCharacter;
    }
    else
    {
      commandOverflowed = true;
    }
  }
}

// Starts serial, then sensors, steppers, and servos. Servo begin() moves them
// to rest. No homing is done: the firmware assumes lead at bottom, barrel at
// index 0, yaw at 0 degrees.
void setup()
{
  Serial.begin(Config::SERIAL_BAUD);

  sensors.begin();
  stepperController.begin();
  servoController.begin();

  Serial.println(
    F("HDM Motor and Servo Controller Ready.")
  );

  printCommands();
}

// Only polls the serial port; all work happens inside commands.
void loop()
{
  readSerialCommands();
}