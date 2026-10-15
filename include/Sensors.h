#pragma once

#include <Arduino.h>

#include "Config.h"

// Non-motor hardware: limit/presence switches, the burnwire trigger, and the
// solenoid that locks the barrel to the yaw mechanism.
class Sensors {
public:
  // Configures every switch input and output pin; outputs start off.
  void begin();

  // Active-low switch reads; always false when the pin is UNUSED_PIN.
  bool isLeadTopSwitchTripped() const;
  bool isLeadBottomSwitchTripped() const;

  // The top switch only needs to trip once: the puck stays "detected" until
  // clearPuckLatch() is called (after the servos fire).
  void updatePuckLatch();
  bool isPuckLatched() const;
  void clearPuckLatch();

  // Both lead top and bottom use active-low switches; the puck switch is optional.
  bool hasPuckSwitch() const;
  // True when a puck is at the firing position (assumed present without a switch).
  bool isPuckInBarrel() const;
  // True when the elevator reaches the lower limit.
  bool isElevatorAtBottom() const;

  // Pulses the burnwire trigger for the given duration (blocking).
  void fireBurnwire(unsigned long durationMs);

  // Energizes the solenoid so the barrel is free to rotate relative to the yaw.
  void unlockBarrel();
  // De-energizes the solenoid so the barrel is locked to the yaw again.
  void lockBarrel();
  bool isBarrelUnlocked() const;

  // Prints switch and solenoid states.
  void printStatus(Stream &output) const;

private:
  bool barrelUnlocked = false;
  bool puckLatched = false;
};
