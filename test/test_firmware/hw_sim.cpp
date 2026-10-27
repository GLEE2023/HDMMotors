#include "hw_sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Arduino.h>
#include <SPI.h>
#include <Servo.h>
#include <TMCStepper.h>

namespace hw {

std::vector<Event> events;
uint8_t pinLevel[256];
int pinModeOf[256];
unsigned long clockMs = 0;
unsigned long clockUs = 0;
unsigned long idlePolls = 0;
int spiBeginCalls = 0;
std::vector<int> servoAttachPins;
TmcState tmc;
std::function<void(uint8_t, uint8_t)> onPinWrite;

void reset() {
  events.clear();
  memset(pinLevel, HIGH, sizeof(pinLevel));
  for (int pin = 0; pin < 256; ++pin) {
    pinModeOf[pin] = -1;
  }
  clockMs = 0;
  clockUs = 0;
  idlePolls = 0;
  spiBeginCalls = 0;
  servoAttachPins.clear();
  tmc = TmcState{0, -1, -1, -1, -1, 0, 0};
  onPinWrite = nullptr;
}

long pulseCount(uint8_t stepPin, size_t upTo) {
  long pulses = 0;
  uint8_t previous = LOW;

  for (size_t i = 0; i < events.size() && i < upTo; ++i) {
    const Event &event = events[i];
    if (event.kind != Kind::PinWrite || event.a != stepPin) {
      continue;
    }
    if (event.b == HIGH && previous == LOW) {
      ++pulses;
    }
    previous = static_cast<uint8_t>(event.b);
  }

  return pulses;
}

long signedPulses(
  uint8_t stepPin,
  uint8_t dirPin,
  uint8_t positiveLevel,
  size_t upTo
) {
  long total = 0;
  uint8_t stepPrevious = LOW;
  uint8_t direction = LOW;

  for (size_t i = 0; i < events.size() && i < upTo; ++i) {
    const Event &event = events[i];
    if (event.kind != Kind::PinWrite) {
      continue;
    }
    if (event.a == dirPin) {
      direction = static_cast<uint8_t>(event.b);
    } else if (event.a == stepPin) {
      if (event.b == HIGH && stepPrevious == LOW) {
        total += direction == positiveLevel ? 1 : -1;
      }
      stepPrevious = static_cast<uint8_t>(event.b);
    }
  }

  return total;
}

bool wroteTo(uint8_t pin) {
  for (const Event &event : events) {
    if (event.kind == Kind::PinWrite && event.a == pin) {
      return true;
    }
  }
  return false;
}

long firstWrite(uint8_t pin, uint8_t level, size_t from) {
  for (size_t i = from; i < events.size(); ++i) {
    if (events[i].kind == Kind::PinWrite && events[i].a == pin &&
        events[i].b == level) {
      return static_cast<long>(i);
    }
  }
  return -1;
}

long lastWrite(uint8_t pin, uint8_t level) {
  for (size_t i = events.size(); i > 0; --i) {
    const Event &event = events[i - 1];
    if (event.kind == Kind::PinWrite && event.a == pin && event.b == level) {
      return static_cast<long>(i - 1);
    }
  }
  return -1;
}

long servoWrites(int pin, int angle) {
  long count = 0;
  for (const Event &event : events) {
    if (event.kind == Kind::ServoWrite && event.a == pin && event.b == angle) {
      ++count;
    }
  }
  return count;
}

int lastServoAngle(int pin) {
  for (size_t i = events.size(); i > 0; --i) {
    const Event &event = events[i - 1];
    if (event.kind == Kind::ServoWrite && event.a == pin) {
      return static_cast<int>(event.b);
    }
  }
  return -1;
}

long firstServoWrite(int pin, int angle, size_t from) {
  for (size_t i = from; i < events.size(); ++i) {
    if (events[i].kind == Kind::ServoWrite && events[i].a == pin &&
        events[i].b == angle) {
      return static_cast<long>(i);
    }
  }
  return -1;
}

unsigned long totalDelayMs() {
  unsigned long total = 0;
  for (const Event &event : events) {
    if (event.kind == Kind::DelayMs) {
      total += static_cast<unsigned long>(event.a);
    }
  }
  return total;
}

}  // namespace hw

// ---------------------------------------------------------------------------
// Arduino core fakes
// ---------------------------------------------------------------------------

void pinMode(uint8_t pin, uint8_t mode) {
  hw::pinModeOf[pin] = mode;
}

void digitalWrite(uint8_t pin, uint8_t level) {
  hw::idlePolls = 0;
  hw::pinLevel[pin] = level ? HIGH : LOW;
  hw::events.push_back({hw::Kind::PinWrite, pin, hw::pinLevel[pin]});

  if (hw::onPinWrite) {
    hw::onPinWrite(pin, hw::pinLevel[pin]);
  }
}

int digitalRead(uint8_t pin) {
  return hw::pinLevel[pin];
}

void delay(unsigned long milliseconds) {
  hw::idlePolls = 0;
  hw::clockMs += milliseconds;
  hw::events.push_back(
    {hw::Kind::DelayMs, static_cast<long>(milliseconds), 0}
  );
}

void delayMicroseconds(unsigned int microseconds) {
  hw::idlePolls = 0;
  hw::clockUs += microseconds;
  hw::events.push_back(
    {hw::Kind::DelayUs, static_cast<long>(microseconds), 0}
  );
}

unsigned long millis() {
  return hw::clockMs + hw::clockUs / 1000UL;
}

uint8_t Servo::attach(int pin) {
  attachedPin = pin;
  hw::servoAttachPins.push_back(pin);
  return 1;
}

void Servo::write(int angle) {
  hw::events.push_back({hw::Kind::ServoWrite, attachedPin, angle});
}

SPIClass SPI;

void SPIClass::begin() {
  ++hw::spiBeginCalls;
}

TMC5160Stepper::TMC5160Stepper(uint16_t chipSelectPin, float rSense)
  : chipSelect(chipSelectPin), senseResistor(rSense) {}

void TMC5160Stepper::begin() { ++hw::tmc.beginCalls; }
void TMC5160Stepper::toff(uint8_t value) { hw::tmc.toff = value; }
void TMC5160Stepper::rms_current(uint16_t milliamps) {
  hw::tmc.rmsCurrentMa = milliamps;
}
void TMC5160Stepper::microsteps(uint16_t value) { hw::tmc.microsteps = value; }
void TMC5160Stepper::en_pwm_mode(bool enabled) {
  hw::tmc.enPwmMode = enabled ? 1 : 0;
}
uint8_t TMC5160Stepper::test_connection() {
  ++hw::tmc.connectionTests;
  return hw::tmc.testResult;
}

// ---------------------------------------------------------------------------
// Print / Serial
// ---------------------------------------------------------------------------

size_t Print::writeText(const std::string &text) {
  for (char character : text) {
    write(static_cast<uint8_t>(character));
  }
  return text.size();
}

size_t Print::print(const __FlashStringHelper *text) {
  return writeText(reinterpret_cast<const char *>(text));
}
size_t Print::print(const char *text) { return writeText(text); }
size_t Print::print(char character) { return write(character); }
size_t Print::print(unsigned char value, int) {
  return writeText(std::to_string(static_cast<unsigned int>(value)));
}
size_t Print::print(int value, int) { return writeText(std::to_string(value)); }
size_t Print::print(unsigned int value, int) {
  return writeText(std::to_string(value));
}
size_t Print::print(long value, int) { return writeText(std::to_string(value)); }
size_t Print::print(unsigned long value, int) {
  return writeText(std::to_string(value));
}
size_t Print::print(double value, int digits) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), "%.*f", digits, value);
  return writeText(buffer);
}

size_t Print::println() { return writeText("\n"); }
size_t Print::println(const __FlashStringHelper *text) {
  return print(text) + println();
}
size_t Print::println(const char *text) { return print(text) + println(); }
size_t Print::println(char character) { return print(character) + println(); }
size_t Print::println(unsigned char value, int base) {
  return print(value, base) + println();
}
size_t Print::println(int value, int base) {
  return print(value, base) + println();
}
size_t Print::println(unsigned int value, int base) {
  return print(value, base) + println();
}
size_t Print::println(long value, int base) {
  return print(value, base) + println();
}
size_t Print::println(unsigned long value, int base) {
  return print(value, base) + println();
}
size_t Print::println(double value, int digits) {
  return print(value, digits) + println();
}

SerialPort Serial;

void SerialPort::begin(unsigned long baud) { baudRate = baud; }

size_t SerialPort::write(uint8_t character) {
  outputText.push_back(static_cast<char>(character));
  return 1;
}

// The firmware busy-waits for the fire confirmation; fail loudly instead of
// hanging the test run if a test forgot to feed an answer.
int SerialPort::available() {
  if (inputBytes.empty() && ++hw::idlePolls > 1000000UL) {
    fprintf(stderr, "FATAL: firmware is blocked waiting for serial input\n");
    abort();
  }
  return static_cast<int>(inputBytes.size());
}

int SerialPort::read() {
  if (inputBytes.empty()) {
    return -1;
  }
  const unsigned char value = static_cast<unsigned char>(inputBytes.front());
  inputBytes.pop_front();
  return value;
}

int SerialPort::peek() {
  return inputBytes.empty() ? -1
                            : static_cast<unsigned char>(inputBytes.front());
}

void SerialPort::feed(const std::string &text) {
  for (char character : text) {
    inputBytes.push_back(character);
  }
}

const std::string &SerialPort::output() const { return outputText; }
void SerialPort::clearOutput() { outputText.clear(); }
unsigned long SerialPort::baud() const { return baudRate; }

void SerialPort::reset() {
  outputText.clear();
  inputBytes.clear();
  baudRate = 0;
}
