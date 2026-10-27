#pragma once

#include <Arduino.h>

#include "Config.h"

// Non-motor hardware: limit/presence switches, the burnwire trigger, and the
// solenoid that locks the barrel to the yaw mechanism.
class Sensors {
public:
  // Sets up the pins that are configured; burnwire low, solenoid locked.
  void begin();

  // Active-low switch reads; always false when the pin is UNUSED_PIN.
  bool isLeadTopSwitchTripped() const;
  bool isLeadBottomSwitchTripped() const;

  // The top switch only needs to trip once: the puck stays "detected" until
  // clearPuckLatch() is called (after the servos fire).
  void updatePuckLatch();
  bool isPuckLatched() const;
  void clearPuckLatch();

  // Whether the optional hardware pins are configured (not UNUSED_PIN).
  // The puck switch shares the TOP_LIMIT_SWITCH_PIN.
  bool hasPuckSwitch() const;
  bool hasBottomSwitch() const;
  bool hasBarrelSolenoid() const;
  bool hasBurnwireTrigger() const;

  // Blocks for durationMs with the burnwire pin high; no-op if unconfigured.
  void fireBurnwire(unsigned long durationMs);

  // Solenoid drive; both are no-ops if unconfigured.
  void unlockBarrel();
  void lockBarrel();
  // Prints switch and solenoid states.
  void printStatus(Stream &output) const;

private:
  // Commanded solenoid state (there is no feedback), used for settle delays and status.
  bool barrelUnlocked = false;
  bool puckLatched = false;
};
