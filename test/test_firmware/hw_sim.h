#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <limits>
#include <vector>

// Recorded view of everything the firmware does to the fake hardware.
namespace hw {

enum class Kind : uint8_t { PinWrite, ServoWrite, DelayMs, DelayUs };

// PinWrite: a = pin, b = level.  ServoWrite: a = servo pin, b = angle.
// DelayMs / DelayUs: a = duration.
struct Event {
  Kind kind;
  long a;
  long b;
};

struct TmcState {
  int beginCalls;
  int toff;
  int rmsCurrentMa;
  int microsteps;
  int enPwmMode;  // -1 = never set
  uint8_t testResult;
  int connectionTests;
};

constexpr size_t kEnd = std::numeric_limits<size_t>::max();

extern std::vector<Event> events;
extern uint8_t pinLevel[256];  // every pin reads HIGH until driven or tripped
extern int pinModeOf[256];     // -1 = never configured
extern unsigned long clockMs;
extern unsigned long clockUs;
extern unsigned long idlePolls;
extern int spiBeginCalls;
extern std::vector<int> servoAttachPins;
extern TmcState tmc;

// Called after every digitalWrite; tests use it to simulate switches or to
// type into the serial port in the middle of a move.
extern std::function<void(uint8_t pin, uint8_t level)> onPinWrite;

void reset();

// Pulses = LOW->HIGH transitions on stepPin among the first `upTo` events.
long pulseCount(uint8_t stepPin, size_t upTo = kEnd);

// Pulses counted +1 when dirPin was at positiveLevel and -1 otherwise.
long signedPulses(
  uint8_t stepPin,
  uint8_t dirPin,
  uint8_t positiveLevel,
  size_t upTo = kEnd
);

// True if the firmware ever wrote to the pin.
bool wroteTo(uint8_t pin);

// Index of the first/last PinWrite of `level` to `pin`, or -1.
long firstWrite(uint8_t pin, uint8_t level, size_t from = 0);
long lastWrite(uint8_t pin, uint8_t level);

// Number of writes of `angle` to a servo.
long servoWrites(int pin, int angle);
// Last angle written to a servo, or -1.
int lastServoAngle(int pin);
// Index of the first write of `angle` to a servo, or -1.
long firstServoWrite(int pin, int angle, size_t from = 0);

// Sum of all delay() calls, in milliseconds.
unsigned long totalDelayMs();

}  // namespace hw
