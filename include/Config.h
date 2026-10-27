#pragma once

#include <Arduino.h>

// Shared hardware constants and motion settings for the whole firmware.
namespace Config {

  // ==========================================================
  // GENERAL DRIVER SETTINGS
  // ==========================================================

  constexpr float R_SENSE = 0.075f;
  constexpr unsigned long SERIAL_BAUD = 115200;

  constexpr uint8_t UNUSED_PIN = 255;

  // Optional pins are written HDM_OPTIONAL_PIN(benchPin, testPin). On hardware
  // this is just benchPin: edit the first argument to wire a pin. Host unit
  // tests (HDM_UNIT_TEST) ignore the bench wiring: every optional pin is
  // UNUSED_PIN, or testPin when HDM_TEST_PINS_CONFIGURED is also defined.
#if defined(HDM_UNIT_TEST) && defined(HDM_TEST_PINS_CONFIGURED)
  #define HDM_OPTIONAL_PIN(benchPin, testPin) (testPin)
#elif defined(HDM_UNIT_TEST)
  #define HDM_OPTIONAL_PIN(benchPin, testPin) (UNUSED_PIN)
#else
  #define HDM_OPTIONAL_PIN(benchPin, testPin) (benchPin)
#endif

  // Lead-screw TMC5160 pins
  constexpr uint8_t LEAD_STEP_PIN = 6;
  constexpr uint8_t LEAD_DIRECTION_PIN = 7;
  constexpr uint8_t LEAD_ENABLE_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 20);
  constexpr uint8_t LEAD_CHIP_SELECT_PIN = 10;

  // Standalone TMC2209 STEP/DIR pins
  constexpr uint8_t BARREL_STEP_PIN = 4;
  constexpr uint8_t BARREL_DIRECTION_PIN = 5;
  constexpr uint8_t BARREL_ENABLE_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 21);

  constexpr uint8_t YAW_STEP_PIN = 2;
  constexpr uint8_t YAW_DIRECTION_PIN = 3;
  constexpr uint8_t YAW_ENABLE_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 22);

  // Burnwire trigger output
  constexpr uint8_t BURNWIRE_TRIGGER_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 23);

  // Solenoid that unlocks the barrel from the yaw while the barrel rotates
  constexpr uint8_t BARREL_SOLENOID_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 24);
  constexpr uint8_t BARREL_SOLENOID_ACTIVE_LEVEL = HIGH;
  constexpr uint16_t BARREL_SOLENOID_SETTLE_TIME_MS = 50;

  // Servo pins
  constexpr uint8_t LEFT_SERVO_PIN = A1;
  constexpr uint8_t RIGHT_SERVO_PIN = A2;

  // Limit-switch pins
  constexpr uint8_t BOTTOM_LIMIT_SWITCH_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 25);
  constexpr uint8_t TOP_LIMIT_SWITCH_PIN = HDM_OPTIONAL_PIN(UNUSED_PIN, 26);

  // General step timing
  constexpr uint8_t STEP_HIGH_TIME_US = 5;
  constexpr uint8_t DIRECTION_SETUP_DELAY_MS = 5;
  constexpr uint8_t ENABLE_SETTLE_TIME_MS = 2;

  enum class MotorId : uint8_t {
    None,
    LeadScrew,
    Barrel,
    Yaw
  };

  struct MotionProfile {
    uint16_t startingDelayUs;
    uint16_t cruiseDelayUs;
    uint16_t accelerationSteps;
  };

  // ==========================================================
  // LEAD-SCREW CONFIGURATION
  // ==========================================================

  constexpr float PUCK_HEIGHT_MM = 14.0f;
  constexpr float BARREL_HEIGHT_MM = 256.0f;
  constexpr uint8_t PUCK_COUNT = 17;
  constexpr float FIRST_PUCK_EXTRA_OFFSET_MM = 13.0f;
  constexpr float FULL_DEPLOYMENT_PRE_FIRE_LOWER_MM = 3.0f;
  constexpr float FULL_DEPLOYMENT_POST_FIRE_RAISE_MM = 3.0f;

  constexpr uint16_t LEAD_CURRENT_MA = 1400;
  constexpr uint16_t LEAD_MICROSTEPS = 4;
  constexpr float LEAD_STEPS_PER_MM = 100.8f;
  constexpr bool LEAD_POSITIVE_DIRECTION_LEVEL = true;

  constexpr MotionProfile LEAD_PROFILE = {
    2400,
    1400,
    300
  };

  // ==========================================================
  // SERVO DEPLOYMENT SETTINGS
  // ==========================================================

  constexpr int LEFT_SERVO_REST = 180;
  constexpr int RIGHT_SERVO_REST = 0;

  constexpr int LEFT_SERVO_ARM = 135;
  constexpr int RIGHT_SERVO_ARM = 45;

  constexpr int LEFT_SERVO_FIRE = 40;
  constexpr int RIGHT_SERVO_FIRE = 140;

  constexpr int SERVO_ANGLE_STEP = 5;
  constexpr unsigned long SERVO_STEP_DELAY_MS = 20;
  constexpr unsigned long SERVO_FIRE_HOLD_MS = 300;

  // ==========================================================
  // BARREL CONFIGURATION
  // ==========================================================

  constexpr uint16_t BARREL_MICROSTEPS = 8;
  constexpr long BARREL_MOTOR_FULL_STEPS_PER_REVOLUTION = 200L;
  constexpr uint8_t BARREL_POSITION_COUNT = 6;
  constexpr float BARREL_GEAR_RATIO = 111.0f / 20.0f;
  constexpr bool BARREL_POSITIVE_DIRECTION_LEVEL = true;

  constexpr MotionProfile BARREL_PROFILE = {
    1800,
    700,
    100
  };

  // ==========================================================
  // YAW CONFIGURATION
  // ==========================================================

  constexpr uint16_t YAW_MICROSTEPS = 8;
  constexpr long YAW_MOTOR_FULL_STEPS_PER_REVOLUTION = 200L;
  constexpr float YAW_GEAR_RATIO = 111.0f / 20.0f;
  constexpr float YAW_MINIMUM_DEGREES = -30.0f;
  constexpr float YAW_MAXIMUM_DEGREES = 30.0f;
  constexpr bool YAW_HOLD_AFTER_MOVE = true;
  constexpr bool YAW_POSITIVE_DIRECTION_LEVEL = true;

  constexpr MotionProfile YAW_PROFILE = {
    2400,
    1200,
    160
  };

}