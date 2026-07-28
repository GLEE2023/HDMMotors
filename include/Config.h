#pragma once

#include <Arduino.h>

// Shared hardware constants and motion settings for the whole firmware.
namespace Config {
  // ==========================================================
  // GENERAL TMC5160 SETTINGS
  // ==========================================================

  constexpr float R_SENSE = 0.075f;
  constexpr unsigned long SERIAL_BAUD = 115200;

  constexpr uint8_t UNUSED_PIN = 255;

  // Driver Pin Definitions
  constexpr uint8_t LEAD_STEP_PIN = 2;
  constexpr uint8_t LEAD_DIRECTION_PIN = 3;
  constexpr uint8_t LEAD_ENABLE_PIN = UNUSED_PIN;

  // Standalone TMC2209 STEP/DIR pins for the barrel and yaw axes.
  constexpr uint8_t BARREL_STEP_PIN = 6;
  constexpr uint8_t BARREL_DIRECTION_PIN = 5;
  constexpr uint8_t BARREL_ENABLE_PIN = UNUSED_PIN;

  constexpr uint8_t YAW_STEP_PIN = 8;
  constexpr uint8_t YAW_DIRECTION_PIN = 7;
  constexpr uint8_t YAW_ENABLE_PIN = UNUSED_PIN;

  constexpr uint8_t LEFT_SERVO_PIN = A1;
  constexpr uint8_t RIGHT_SERVO_PIN = A2;

  // Limit Switch Pins
  constexpr uint8_t BOTTOM_LIMIT_SWITCH_PIN = A4;
  constexpr uint8_t TOP_BARREL_SWITCH_PIN = A5;

  // SPI CS Pin for the lead-screw TMC5160 only.
  constexpr uint8_t LEAD_CHIP_SELECT_PIN = 10;

  constexpr uint8_t STEP_HIGH_TIME_US = 5;
  constexpr uint8_t DIRECTION_SETUP_TIME_US = 20;
  constexpr uint8_t ENABLE_SETTLE_TIME_MS = 2;

  // Identifies the three motion axes so commands can target the correct motor.
  enum class MotorId : uint8_t {
    None,
    LeadScrew,
    Barrel,
    Yaw
  };

  // Stores the timing values used to shape a stepper move profile.
  struct MotionProfile {
    uint16_t startingDelayUs;
    uint16_t cruiseDelayUs;
    uint16_t accelerationSteps;
  };

  // ==========================================================
  // LEAD-SCREW CONFIGURATION
  // ==========================================================

  // ==========================================================
  // LEAD-SCREW CONFIGURATION
  // ==========================================================

  constexpr float PUCK_HEIGHT_MM = 14.0f;
  constexpr float BARREL_HEIGHT_MM = 250.0f;
  constexpr uint8_t PUCK_COUNT = 17;
  constexpr float FIRST_PUCK_EXTRA_OFFSET_MM = 10.0f;

  constexpr uint16_t LEAD_CURRENT_MA = 1400;
  constexpr uint16_t LEAD_MICROSTEPS = 4;
  constexpr float LEAD_STEPS_PER_MM = 100.8f;
  constexpr bool LEAD_POSITIVE_DIRECTION_LEVEL = false;

  // Matches the speed used by the working standalone program
  constexpr MotionProfile LEAD_PROFILE = {
    2400,
    1400,
    300
  };;

  // ==========================================================
  // SERVO DEPLOYMENT SETTINGS
  // ==========================================================

  constexpr int LEFT_SERVO_REST = 180;
  constexpr int RIGHT_SERVO_REST = 0;

  constexpr int LEFT_SERVO_ARM = 135;
  constexpr int RIGHT_SERVO_ARM = 45;


  constexpr int LEFT_SERVO_FIRE = 40;
  constexpr int RIGHT_SERVO_FIRE = 140;
=======
  constexpr int LEFT_SERVO_FIRE = 60;
  constexpr int RIGHT_SERVO_FIRE = 130;


  constexpr int SERVO_ANGLE_STEP = 5;
  constexpr unsigned long SERVO_STEP_DELAY_MS = 20;
  constexpr unsigned long SERVO_FIRE_HOLD_MS = 300;

  // ==========================================================
  // BARREL & YAW SETTINGS
  // ==========================================================

  constexpr uint16_t BARREL_CURRENT_MA = 600;
  // Standalone TMC2209s use the physical MS1/MS2 wiring for microstep resolution.
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

  constexpr uint16_t YAW_CURRENT_MA = 600;
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