#include "Sensors.h"

// Configures only the pins that are set: switches as INPUT_PULLUP (active-low),
// burnwire and solenoid as outputs (burnwire low). Ends by locking the barrel.
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

// Switch readers: true only if the pin is configured and reads LOW. An
// unconfigured switch always reads "not tripped". The "top" switch is the puck
// switch (same pin).
bool Sensors::isLeadTopSwitchTripped() const {
  return Config::TOP_LIMIT_SWITCH_PIN != Config::UNUSED_PIN &&
    digitalRead(Config::TOP_LIMIT_SWITCH_PIN) == LOW;
}

bool Sensors::isLeadBottomSwitchTripped() const {
  return hasBottomSwitch() &&
    digitalRead(Config::BOTTOM_LIMIT_SWITCH_PIN) == LOW;
}

// True if a puck/top switch pin is configured.
bool Sensors::hasPuckSwitch() const {
  return Config::TOP_LIMIT_SWITCH_PIN != Config::UNUSED_PIN;
}

bool Sensors::hasBottomSwitch() const {
  return Config::BOTTOM_LIMIT_SWITCH_PIN != Config::UNUSED_PIN;
}

bool Sensors::hasBarrelSolenoid() const {
  return Config::BARREL_SOLENOID_PIN != Config::UNUSED_PIN;
}

// Puck latch: set once the puck switch is seen tripped, until cleared. Polled
// per step during lead moves, and by the deployment sequence.
void Sensors::updatePuckLatch() {
  if (isLeadTopSwitchTripped()) {
    puckLatched = true;
  }
}

bool Sensors::isPuckLatched() const {
  return puckLatched;
}

void Sensors::clearPuckLatch() {
  puckLatched = false;
}

bool Sensors::hasBurnwireTrigger() const {
  return Config::BURNWIRE_TRIGGER_PIN != Config::UNUSED_PIN;
}

// Drives the burnwire pin high for durationMs (blocking delay), then low.
// Does nothing if the pin is unconfigured; callers check hasBurnwireTrigger()
// first so they can report that no pulse was sent.
void Sensors::fireBurnwire(unsigned long durationMs) {
  if (!hasBurnwireTrigger()) {
    return;
  }

  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
}

// Sets the solenoid to its active level; waits the settle time only if it was
// previously locked. No-op if no solenoid is configured.
void Sensors::unlockBarrel() {
  if (!hasBarrelSolenoid()) {
    return;
  }

  digitalWrite(
    Config::BARREL_SOLENOID_PIN,
    Config::BARREL_SOLENOID_ACTIVE_LEVEL
  );

  if (!barrelUnlocked) {
    delay(Config::BARREL_SOLENOID_SETTLE_TIME_MS);
  }

  barrelUnlocked = true;
}

// Sets the solenoid to its inactive level; waits the settle time only if it was
// previously unlocked. No-op if no solenoid is configured.
void Sensors::lockBarrel() {
  if (!hasBarrelSolenoid()) {
    return;
  }

  digitalWrite(
    Config::BARREL_SOLENOID_PIN,
    !Config::BARREL_SOLENOID_ACTIVE_LEVEL
  );

  if (barrelUnlocked) {
    delay(Config::BARREL_SOLENOID_SETTLE_TIME_MS);
  }

  barrelUnlocked = false;
}

// Prints live switch states and the commanded (not sensed) solenoid state.
void Sensors::printStatus(Stream &output) const {
  output.print(F("Puck in Barrel Switch: "));
  if (hasPuckSwitch()) {
    output.println(isLeadTopSwitchTripped() ? F("TRIGGERED") : F("OPEN"));
  }
  else {
    output.println(F("NOT CONFIGURED (presence assumed)"));
  }

  output.print(F("Elevator Bottom Switch: "));
  if (hasBottomSwitch()) {
    output.println(isLeadBottomSwitchTripped() ? F("TRIGGERED") : F("OPEN"));
  }
  else {
    output.println(F("NOT CONFIGURED"));
  }

  output.print(F("Barrel Solenoid: "));
  if (hasBarrelSolenoid()) {
    output.println(barrelUnlocked ? F("UNLOCKED") : F("LOCKED"));
  }
  else {
    output.println(F("NOT CONFIGURED"));
  }
}
