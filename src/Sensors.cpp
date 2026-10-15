#include "Sensors.h"

void Sensors::begin() {
  if (Config::BOTTOM_LIMIT_SWITCH_PIN != Config::UNUSED_PIN) {
    pinMode(Config::BOTTOM_LIMIT_SWITCH_PIN, INPUT_PULLUP);
  }

  if (Config::TOP_LIMIT_SWITCH_PIN != Config::UNUSED_PIN) {
    pinMode(Config::TOP_LIMIT_SWITCH_PIN, INPUT_PULLUP);
  }

  if (Config::BURNWIRE_TRIGGER_PIN != Config::UNUSED_PIN) {
    pinMode(Config::BURNWIRE_TRIGGER_PIN, OUTPUT);
    digitalWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
  }

  if (Config::BARREL_SOLENOID_PIN != Config::UNUSED_PIN) {
    pinMode(Config::BARREL_SOLENOID_PIN, OUTPUT);
  }

  lockBarrel();
}

bool Sensors::isLeadTopSwitchTripped() const {
  return Config::TOP_LIMIT_SWITCH_PIN != Config::UNUSED_PIN &&
    digitalRead(Config::TOP_LIMIT_SWITCH_PIN) == LOW;
}

bool Sensors::isLeadBottomSwitchTripped() const {
  return Config::BOTTOM_LIMIT_SWITCH_PIN != Config::UNUSED_PIN &&
    digitalRead(Config::BOTTOM_LIMIT_SWITCH_PIN) == LOW;
}

bool Sensors::isPuckInBarrel() const {
  // Without a switch the puck cannot be verified, so assume one is present.
  if (Config::TOP_LIMIT_SWITCH_PIN == Config::UNUSED_PIN) {
    return true;
  }

  return isLeadTopSwitchTripped();
}

bool Sensors::isElevatorAtBottom() const {
  return isLeadBottomSwitchTripped();
}

// Activates the burnwire for a short pulse using the transistor driver.
void Sensors::fireBurnwire(unsigned long durationMs) {
  if (Config::BURNWIRE_TRIGGER_PIN == Config::UNUSED_PIN) {
    return;
  }

  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
}

void Sensors::unlockBarrel() {
  if (Config::BARREL_SOLENOID_PIN != Config::UNUSED_PIN) {
    digitalWrite(
      Config::BARREL_SOLENOID_PIN,
      Config::BARREL_SOLENOID_ACTIVE_LEVEL
    );
  }

  if (!barrelUnlocked) {
    delay(Config::BARREL_SOLENOID_SETTLE_TIME_MS);
  }

  barrelUnlocked = true;
}

void Sensors::lockBarrel() {
  if (Config::BARREL_SOLENOID_PIN != Config::UNUSED_PIN) {
    digitalWrite(
      Config::BARREL_SOLENOID_PIN,
      !Config::BARREL_SOLENOID_ACTIVE_LEVEL
    );
  }

  if (barrelUnlocked) {
    delay(Config::BARREL_SOLENOID_SETTLE_TIME_MS);
  }

  barrelUnlocked = false;
}

bool Sensors::isBarrelUnlocked() const {
  return barrelUnlocked;
}

void Sensors::printStatus(Stream &output) const {
  output.print(F("Puck in Barrel Switch: "));
  output.println(isPuckInBarrel() ? F("TRIGGERED") : F("OPEN"));

  output.print(F("Elevator Bottom Switch: "));
  output.println(isElevatorAtBottom() ? F("TRIGGERED") : F("OPEN"));

  output.print(F("Barrel Solenoid: "));
  output.println(barrelUnlocked ? F("UNLOCKED") : F("LOCKED"));
}
