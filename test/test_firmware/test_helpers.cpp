#include "test_helpers.h"

#include <memory>
#include <new>
#include <vector>

namespace {

std::vector<std::function<void(uint8_t, uint8_t)>> listeners;

void dispatch(uint8_t pin, uint8_t level) {
  for (auto &listener : listeners) {
    listener(pin, level);
  }
}

template <typename T, typename... Args>
void rebuild(T &object, Args &...args) {
  object.~T();
  new (&object) T(args...);
}

}  // namespace

namespace t {

void resetFirmware() {
  hw::reset();
  Serial.reset();
  listeners.clear();
  hw::onPinWrite = dispatch;

  rebuild(sensors);
  rebuild(stepperController, sensors);
  rebuild(axes, stepperController, sensors);
  rebuild(servoController);

  // Attach the servos as setup() would, without keeping its log entries.
  servoController.begin();
  hw::events.clear();
  hw::servoAttachPins.clear();
  hw::clockMs = 0;
  hw::clockUs = 0;

  commandLength = 0;
  commandOverflowed = false;
  commandBuffer[0] = '\0';
}

void addPinListener(std::function<void(uint8_t, uint8_t)> listener) {
  listeners.push_back(listener);
}

void typeAfterPulses(uint8_t stepPin, long pulses, const std::string &text) {
  auto seen = std::make_shared<long>(0);
  auto level = std::make_shared<uint8_t>(LOW);

  addPinListener([=](uint8_t pin, uint8_t value) {
    if (pin != stepPin) {
      return;
    }
    if (value == HIGH && *level == LOW && ++*seen == pulses) {
      Serial.feed(text);
    }
    *level = value;
  });
}

void simulateLeadScrew(long startSteps, std::function<bool(long)> puckClosed) {
  auto position = std::make_shared<long>(startSteps);
  auto direction = std::make_shared<uint8_t>(LOW);
  auto stepLevel = std::make_shared<uint8_t>(LOW);

  auto refreshSwitches = [=]() {
    if (kBottomSwitch) {
      hw::pinLevel[Config::BOTTOM_LIMIT_SWITCH_PIN] =
        *position <= 0 ? LOW : HIGH;
    }
    if (kPuckSwitch && puckClosed) {
      hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] =
        puckClosed(*position) ? LOW : HIGH;
    }
  };

  refreshSwitches();

  addPinListener([=](uint8_t pin, uint8_t value) {
    if (pin == Config::LEAD_DIRECTION_PIN) {
      *direction = value;
    } else if (pin == Config::LEAD_STEP_PIN) {
      if (value == HIGH && *stepLevel == LOW) {
        *position +=
          *direction == Config::LEAD_POSITIVE_DIRECTION_LEVEL ? 1 : -1;
        refreshSwitches();
      }
      *stepLevel = value;
    }
  });
}

long leadNet(size_t upTo) {
  return hw::signedPulses(
    Config::LEAD_STEP_PIN,
    Config::LEAD_DIRECTION_PIN,
    Config::LEAD_POSITIVE_DIRECTION_LEVEL,
    upTo
  );
}

long barrelNet() {
  return hw::signedPulses(
    Config::BARREL_STEP_PIN,
    Config::BARREL_DIRECTION_PIN,
    Config::BARREL_POSITIVE_DIRECTION_LEVEL
  );
}

long yawNet() {
  return hw::signedPulses(
    Config::YAW_STEP_PIN,
    Config::YAW_DIRECTION_PIN,
    Config::YAW_POSITIVE_DIRECTION_LEVEL
  );
}

long leadPulses() { return hw::pulseCount(Config::LEAD_STEP_PIN); }
long barrelPulses() { return hw::pulseCount(Config::BARREL_STEP_PIN); }
long yawPulses() { return hw::pulseCount(Config::YAW_STEP_PIN); }

const std::string &out() { return Serial.output(); }

bool has(const std::string &text) {
  return Serial.output().find(text) != std::string::npos;
}

int countOf(const std::string &text) {
  int count = 0;
  size_t at = 0;
  while ((at = Serial.output().find(text, at)) != std::string::npos) {
    ++count;
    at += text.size();
  }
  return count;
}

void clearOut() { Serial.clearOutput(); }

void run(const char *command) { processCommand(command); }

}  // namespace t
