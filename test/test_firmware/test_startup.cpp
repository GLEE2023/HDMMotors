// setup() behavior and sanity checks on the Config values the spec relies on.
#include "test_helpers.h"

namespace {

void test_setup_prints_the_banner_and_command_list(void) {
  setup();
  ASSERT_OUT_HAS("HDM Motor and Servo Controller Ready.");
  ASSERT_OUT_HAS("========== COMMANDS ==========");
  EQ(Config::SERIAL_BAUD, Serial.baud());
}

void test_setup_attaches_servos_and_rests_without_moving_motors(void) {
  setup();
  EQ(2, hw::servoAttachPins.size());
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(Config::LEFT_SERVO_PIN));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(Config::RIGHT_SERVO_PIN));
  EQ(0, t::leadPulses() + t::barrelPulses() + t::yawPulses());
  EQ(1, hw::tmc.beginCalls);
}

void test_startup_assumes_zero_positions(void) {
  setup();
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("(0 steps)");
  ASSERT_OUT_HAS("Lead puck level: 0 / 17");
  ASSERT_OUT_HAS("Barrel index: 0 of 6");
  ASSERT_OUT_HAS("Yaw position: 0 steps");
}

void test_help_lists_every_command(void) {
  printCommands();
  const char *keys[] = {"G / g", "H / h", "w ", "e ", "m / M", "W / S", "+ / -",
                        "N / P", "B ", "I5", "Y12.5", "R0.25", "Z ", "C ",
                        "T / t", "X ", "U ", "? "};
  for (const char *k : keys) {
    ASSERT_OUT_HAS(k);
  }
}

void test_config_geometry_is_consistent(void) {
  const float top = Config::FIRST_PUCK_EXTRA_OFFSET_MM +
                    (Config::PUCK_COUNT - 1) * Config::PUCK_HEIGHT_MM;
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 237.0f, top);
  TEST_ASSERT_TRUE(top <= Config::BARREL_HEIGHT_MM);
  EQ(17, Config::PUCK_COUNT);
  EQ(6, Config::BARREL_POSITION_COUNT);
  TEST_ASSERT_EQUAL_FLOAT(-30.0f, Config::YAW_MINIMUM_DEGREES);
  TEST_ASSERT_EQUAL_FLOAT(30.0f, Config::YAW_MAXIMUM_DEGREES);
}

void test_servo_angles_are_valid(void) {
  const int angles[] = {Config::LEFT_SERVO_REST, Config::RIGHT_SERVO_REST,
                        Config::LEFT_SERVO_ARM, Config::RIGHT_SERVO_ARM,
                        Config::LEFT_SERVO_FIRE, Config::RIGHT_SERVO_FIRE};
  for (int a : angles) {
    TEST_ASSERT_TRUE(a >= 0 && a <= 180);
  }
}

}  // namespace

void run_startup_tests() {
  RUN_TEST(test_setup_prints_the_banner_and_command_list);
  RUN_TEST(test_setup_attaches_servos_and_rests_without_moving_motors);
  RUN_TEST(test_startup_assumes_zero_positions);
  RUN_TEST(test_help_lists_every_command);
  RUN_TEST(test_config_geometry_is_consistent);
  RUN_TEST(test_servo_angles_are_valid);
}
