#pragma once

#include <stddef.h>

#include <functional>
#include <string>

#include <unity.h>

#include "Axes.h"
#include "Config.h"
#include "Sensors.h"
#include "ServoController.h"
#include "StepperController.h"
#include "hw_sim.h"

// Firmware globals and entry points defined in src/main.cpp.
extern Sensors sensors;
extern StepperController stepperController;
extern Axes axes;
extern ServoController servoController;
extern char commandBuffer[32];
extern uint8_t commandLength;
extern bool commandOverflowed;

void setup();
void loop();
void processCommand(const char *command);
void readSerialCommands();
void printCommands();
void returnLeadToBottom();
bool waitForFireConfirmation();
void executeFullDeployment();
bool parseLongValue(const char *text, long &value);
bool parseFloatValue(const char *text, float &value);

namespace t {

constexpr bool kBottomSwitch =
  Config::BOTTOM_LIMIT_SWITCH_PIN != Config::UNUSED_PIN;
constexpr bool kPuckSwitch =
  Config::TOP_LIMIT_SWITCH_PIN != Config::UNUSED_PIN;
constexpr bool kSolenoid = Config::BARREL_SOLENOID_PIN != Config::UNUSED_PIN;
constexpr bool kBurnwire = Config::BURNWIRE_TRIGGER_PIN != Config::UNUSED_PIN;

// Expected lead-screw step counts, worked out by hand from Config.h
// (100.8 steps/mm, 13 mm first-puck offset, 14 mm per puck, 256 mm travel).
constexpr long kFullTravelSteps = 25805;  // 256 mm
constexpr long kOneMmSteps = 101;         // 1 mm
constexpr long kPuckHeightSteps = 1411;   // 14 mm
constexpr long kThreeMmSteps = 302;       // 3 mm
constexpr long kLevelOneSteps = 1310;     // 13 mm
constexpr long kLevelTwoSteps = 2722;     // 27 mm

// Barrel: 200 * 8 * (111/20) = 8880 microsteps/rev, 6 indexes -> 1480 each.
constexpr long kBarrelRevolutionSteps = 8880;
constexpr long kBarrelIndexSteps = 1480;

// Yaw: 200 * 8 * (111/20) / 360 = 24.667 steps/degree.
constexpr long kYawTenDegreeSteps = 247;
constexpr long kYawLimitSteps = 740;  // 30 degrees

constexpr unsigned long kBurnwireMs = 12000;

// Resets the fake hardware, the serial port and every firmware global.
void resetFirmware();

// Listeners run after each digitalWrite (several tests combine them).
void addPinListener(std::function<void(uint8_t pin, uint8_t level)> listener);

// Types `text` into the serial port once stepPin has produced `pulses` pulses.
void typeAfterPulses(uint8_t stepPin, long pulses, const std::string &text);

// Simulated lead screw: pulses move a physical position, the bottom switch
// closes (LOW) at or below 0, and the puck switch closes where puckClosed says.
void simulateLeadScrew(
  long startSteps,
  std::function<bool(long steps)> puckClosed = nullptr
);

// Net lead / barrel / yaw steps taken so far (up to event index `upTo`).
long leadNet(size_t upTo = hw::kEnd);
long barrelNet();
long yawNet();
long leadPulses();
long barrelPulses();
long yawPulses();

const std::string &out();
bool has(const std::string &text);
int countOf(const std::string &text);
void clearOut();
void run(const char *command);

}  // namespace t

#define EQ(expected, actual) \
  TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))

#define ASSERT_OUT_HAS(text) \
  TEST_ASSERT_TRUE_MESSAGE( \
    t::has(text), \
    (std::string("expected output to contain: ") + (text) + "\n--- output ---\n" + t::out()).c_str())

#define ASSERT_OUT_LACKS(text) \
  TEST_ASSERT_FALSE_MESSAGE( \
    t::has(text), \
    (std::string("expected output NOT to contain: ") + (text) + "\n--- output ---\n" + t::out()).c_str())

void run_parsing_tests();
void run_command_tests();
void run_lead_tests();
void run_barrel_yaw_tests();
void run_servo_tests();
void run_sensor_tests();
void run_stepper_tests();
void run_deployment_tests();
void run_startup_tests();
