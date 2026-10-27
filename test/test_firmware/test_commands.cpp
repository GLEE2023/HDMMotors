// Command reference from the master doc: letters, case rules, arguments,
// prompts, and the simple (non-lead) commands.
#include "test_helpers.h"

namespace {

const int kLeftPin = Config::LEFT_SERVO_PIN;
const int kRightPin = Config::RIGHT_SERVO_PIN;

// --- help / dispatch --------------------------------------------------------

void test_question_mark_prints_every_command(void) {
  t::run("?");
  ASSERT_OUT_HAS("========== COMMANDS ==========");
  const char *const entries[] = {
    "  G / g ", "  H / h ", "  w ", "  e ", "  m / M ", "  W / S ",
    "  + / - ", "  N / P ", "  B ", "  I5 ", "  Y12.5 ", "  R0.25 ",
    "  Z ", "  C ", "  T / t ", "  X ", "  U ", "  ? ",
  };
  for (const char *entry : entries) {
    ASSERT_OUT_HAS(entry);
  }
}

void test_help_describes_h_and_b_differently(void) {
  t::run("?");
  ASSERT_OUT_HAS("Return lead screw to configured bottom");
  ASSERT_OUT_HAS("Drive lead screw down for the full travel");
}

void test_unknown_command_prints_message_and_list(void) {
  t::run("Q");
  ASSERT_OUT_HAS("Unknown command.");
  ASSERT_OUT_HAS("========== COMMANDS ==========");
  EQ(0, t::leadPulses() + t::barrelPulses() + t::yawPulses());
}

void test_uppercase_e_is_unknown(void) {
  t::run("E");
  ASSERT_OUT_HAS("Unknown command.");
  EQ(0, hw::servoWrites(kLeftPin, Config::LEFT_SERVO_ARM));
}

void test_leading_whitespace_before_command_is_ignored(void) {
  t::run("  \t?");
  ASSERT_OUT_HAS("========== COMMANDS ==========");
}

void test_empty_command_is_silent(void) {
  t::run("");
  t::run("   ");
  TEST_ASSERT_EQUAL_STRING("", t::out().c_str());
}

// --- case rules ---------------------------------------------------------------

void test_lowercase_w_resets_servos_and_does_not_raise_elevator(void) {
  t::run("e");
  t::clearOut();
  t::run("w");
  ASSERT_OUT_HAS("Both servos reset.");
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(kLeftPin));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(kRightPin));
  EQ(0, t::leadPulses());
}

void test_uppercase_w_raises_elevator_and_not_servos(void) {
  t::run("W");
  EQ(t::kLevelOneSteps, t::leadNet());
  EQ(-1, hw::lastServoAngle(kLeftPin));
}

void test_lowercase_e_arms_servos(void) {
  t::run("e");
  ASSERT_OUT_HAS("Both servos armed.");
  EQ(Config::LEFT_SERVO_ARM, hw::lastServoAngle(kLeftPin));
  EQ(Config::RIGHT_SERVO_ARM, hw::lastServoAngle(kRightPin));
}

void test_lowercase_letters_work_for_case_insensitive_commands(void) {
  t::run("s");
  ASSERT_OUT_HAS("already at the bottom puck level");
  t::clearOut();

  t::run("b");
  ASSERT_OUT_HAS("Running lead screw down");
  t::clearOut();

  t::run("n");
  ASSERT_OUT_HAS("Moving barrel to index 1");
  t::clearOut();

  t::run("p");
  ASSERT_OUT_HAS("Moving barrel to index 0");
  t::clearOut();

  t::run("i3");
  ASSERT_OUT_HAS("Moving barrel to index 3");
  t::clearOut();

  t::run("y5");
  ASSERT_OUT_HAS("Moving yaw to requested 5.0000");
  t::clearOut();

  t::run("r5");
  ASSERT_OUT_HAS("Moving yaw to requested");
  t::clearOut();

  t::run("z");
  ASSERT_OUT_HAS("Current yaw position set as 0 degrees.");
  t::clearOut();

  t::run("c");
  ASSERT_OUT_HAS("POSITION STATUS");
  t::clearOut();

  t::run("x");
  TEST_ASSERT_TRUE(
    t::has("step pulses stopped") || t::has("All stepper drivers disabled.")
  );
}

void test_lowercase_h_t_u_m_work(void) {
  t::run("h");
  TEST_ASSERT_TRUE(
    t::has("Bottom limit switch") || t::has("Running lead screw down")
  );
  t::clearOut();

  t::run("t");
  ASSERT_OUT_HAS("Lead TMC5160 SPI result");
  t::clearOut();

  t::run("u");
  ASSERT_OUT_HAS("Burnwire");
  t::clearOut();

  t::run("m");
  ASSERT_OUT_HAS("Both servos fired.");
}

// --- servo commands -----------------------------------------------------------

void test_m_fires_both_servos_and_returns_them_to_rest(void) {
  t::run("e");
  t::run("M");
  ASSERT_OUT_HAS("Both servos fired.");
  EQ(1, hw::servoWrites(kLeftPin, Config::LEFT_SERVO_FIRE));
  EQ(1, hw::servoWrites(kRightPin, Config::RIGHT_SERVO_FIRE));
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(kLeftPin));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(kRightPin));
}

void test_m_does_not_move_the_elevator(void) {
  t::run("m");
  EQ(0, t::leadPulses());
}

void test_m_clears_the_puck_latch(void) {
  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = LOW;
  sensors.updatePuckLatch();
  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = HIGH;

  t::run("m");
  TEST_ASSERT_FALSE(sensors.isPuckLatched());
}

void test_m_fires_without_a_puck_present(void) {
  t::run("m");
  ASSERT_OUT_LACKS("No puck");
  EQ(1, hw::servoWrites(kLeftPin, Config::LEFT_SERVO_FIRE));
}

// --- I<n> ---------------------------------------------------------------------

void test_index_argument_forms(void) {
  t::run("I5");
  ASSERT_OUT_HAS("Moving barrel to index 5");
  t::clearOut();
  t::run("I=3");
  ASSERT_OUT_HAS("Moving barrel to index 3");
  t::clearOut();
  t::run("I: 2");
  ASSERT_OUT_HAS("Moving barrel to index 2");
  t::clearOut();
  t::run("I 4");
  ASSERT_OUT_HAS("Moving barrel to index 4");
}

void test_index_out_of_range_is_rejected_without_motion(void) {
  t::run("I6");
  ASSERT_OUT_HAS("Barrel index must be between 0 and 5.");
  t::clearOut();
  t::run("I-1");
  ASSERT_OUT_HAS("Barrel index must be between 0 and 5.");
  t::clearOut();
  t::run("I100");
  ASSERT_OUT_HAS("Barrel index must be between 0 and 5.");
  EQ(0, t::barrelPulses());
}

void test_index_unparsable_is_rejected_without_motion(void) {
  const char *const bad[] = {"I", "Ix", "I5 junk", "I2.5", "I5x", "I:", "I 99999999999999999999"};
  for (const char *command : bad) {
    t::clearOut();
    t::run(command);
    ASSERT_OUT_HAS("Invalid index. Example: I5");
  }
  EQ(0, t::barrelPulses());
}

// --- Y / R --------------------------------------------------------------------

void test_yaw_absolute_argument_forms(void) {
  t::run("Y12.5");
  ASSERT_OUT_HAS("Moving yaw to requested 12.5000");
  EQ(308, t::yawNet());  // 12.5 deg * 24.667 steps/deg
  t::clearOut();
  t::run("Y: 12.5");
  ASSERT_OUT_HAS("Moving yaw to requested 12.5000");
  EQ(308, t::yawNet());  // already there, so no further motion
}

void test_yaw_absolute_is_clamped_to_limits(void) {
  t::run("Y45");
  ASSERT_OUT_HAS("outside the yaw limits; clamped");
  EQ(t::kYawLimitSteps, t::yawNet());
  t::run("Y-45");
  EQ(-t::kYawLimitSteps, t::yawNet());
}

void test_yaw_relative_uses_the_tracked_angle(void) {
  t::run("R-0.25");
  EQ(-6, t::yawNet());  // 0.25 deg * 24.667 = 6.17 steps
  t::clearOut();
  t::run("R=0.25");
  EQ(0, t::yawNet());
}

void test_yaw_unparsable_values_are_rejected_without_motion(void) {
  const char *const badAbsolute[] = {"Y", "Yabc", "Y1.5abc", "Y12 5", "Ynan", "Y1e999"};
  for (const char *command : badAbsolute) {
    t::clearOut();
    t::run(command);
    ASSERT_OUT_HAS("Invalid yaw angle. Example: Y12.5");
  }

  const char *const badRelative[] = {"R", "Rx", "R1x", "R1 2"};
  for (const char *command : badRelative) {
    t::clearOut();
    t::run(command);
    ASSERT_OUT_HAS("Invalid relative yaw. Example: R0.25");
  }

  EQ(0, t::yawPulses());
  EQ(0, t::barrelPulses());
}

// --- Z ------------------------------------------------------------------------

void test_z_declares_yaw_zero_without_moving(void) {
  t::run("Y10");
  EQ(t::kYawTenDegreeSteps, t::yawNet());
  t::clearOut();

  t::run("Z");
  ASSERT_OUT_HAS("Current yaw position set as 0 degrees.");
  EQ(t::kYawTenDegreeSteps, t::yawNet());

  t::run("C");
  ASSERT_OUT_HAS("Yaw position: 0 steps");

  t::run("Y10");
  EQ(2 * t::kYawTenDegreeSteps, t::yawNet());
}

// --- T ------------------------------------------------------------------------

void test_t_reports_the_lead_driver_connection_only(void) {
  t::run("T");
  ASSERT_OUT_HAS("Lead TMC5160 SPI result: 0");
  ASSERT_OUT_HAS("Barrel TMC2209: standalone STEP/DIR; no UART test.");
  ASSERT_OUT_HAS("Yaw TMC2209: standalone STEP/DIR; no UART test.");
  EQ(1, hw::tmc.connectionTests);
}

void test_t_reports_a_nonzero_spi_result(void) {
  hw::tmc.testResult = 3;
  t::run("t");
  ASSERT_OUT_HAS("Lead TMC5160 SPI result: 3");
}

// --- X ------------------------------------------------------------------------

void test_x_drives_every_step_pin_low(void) {
  t::run("X");
  for (uint8_t pin : {Config::LEAD_STEP_PIN, Config::BARREL_STEP_PIN, Config::YAW_STEP_PIN}) {
    TEST_ASSERT_TRUE(hw::wroteTo(pin));
    EQ(LOW, hw::pinLevel[pin]);
  }
}

#ifdef HDM_TEST_PINS_CONFIGURED
void test_x_disables_drivers_that_have_enable_pins(void) {
  t::run("X");
  ASSERT_OUT_HAS("All stepper drivers disabled.");
  ASSERT_OUT_LACKS("holding torque may remain");
  EQ(HIGH, hw::pinLevel[Config::LEAD_ENABLE_PIN]);
  EQ(HIGH, hw::pinLevel[Config::BARREL_ENABLE_PIN]);
  EQ(HIGH, hw::pinLevel[Config::YAW_ENABLE_PIN]);
}
#else
void test_x_does_not_claim_drivers_are_disabled_without_enable_pins(void) {
  t::run("X");
  ASSERT_OUT_HAS("holding torque may remain on");
  ASSERT_OUT_LACKS("All stepper drivers disabled.");
  TEST_ASSERT_FALSE(hw::wroteTo(Config::UNUSED_PIN));
}
#endif

// --- U ------------------------------------------------------------------------

#ifdef HDM_TEST_PINS_CONFIGURED
void test_u_pulses_the_burnwire_for_twelve_seconds(void) {
  t::run("U");
  ASSERT_OUT_HAS("Burnwire pulse started.");
  ASSERT_OUT_HAS("Burnwire pulse complete.");

  const long high = hw::firstWrite(Config::BURNWIRE_TRIGGER_PIN, HIGH);
  const long low = hw::lastWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
  TEST_ASSERT_TRUE(high >= 0);
  TEST_ASSERT_TRUE(low > high);
  EQ(t::kBurnwireMs, hw::totalDelayMs());
  EQ(LOW, hw::pinLevel[Config::BURNWIRE_TRIGGER_PIN]);
}
#else
void test_u_reports_that_no_pulse_was_sent_without_a_trigger_pin(void) {
  t::run("U");
  ASSERT_OUT_HAS("Burnwire trigger pin is not configured; no pulse sent.");
  ASSERT_OUT_LACKS("Burnwire pulse started.");
  ASSERT_OUT_LACKS("Burnwire pulse complete.");
  TEST_ASSERT_TRUE(hw::events.empty());
}
#endif

// --- C ------------------------------------------------------------------------

void test_c_prints_axes_servos_and_sensors(void) {
  t::run("C");
  ASSERT_OUT_HAS("========== POSITION STATUS ==========");
  ASSERT_OUT_HAS("Lead screw: 0.000 / 256.000 mm (0 steps)");
  ASSERT_OUT_HAS("Lead puck level: 0 / 17");
  ASSERT_OUT_HAS("Barrel index: 0 of 6");
  ASSERT_OUT_HAS("Barrel position: 0 / 8880 steps/revolution");
  ASSERT_OUT_HAS("Yaw: 0.000000 degrees");
  ASSERT_OUT_HAS("Yaw allowed range: -30.0 to +30.0 degrees");
  ASSERT_OUT_HAS("Left servo: 180  Right servo: 0");
  ASSERT_OUT_HAS("Puck in Barrel Switch:");
  ASSERT_OUT_HAS("Elevator Bottom Switch:");
  ASSERT_OUT_HAS("Barrel Solenoid:");
}

#ifdef HDM_TEST_PINS_CONFIGURED
void test_c_shows_configured_hardware_states(void) {
  t::run("C");
  ASSERT_OUT_HAS("Puck in Barrel Switch: OPEN");
  ASSERT_OUT_HAS("Elevator Bottom Switch: OPEN");
  ASSERT_OUT_HAS("Barrel Solenoid: LOCKED");
  ASSERT_OUT_LACKS("NOT CONFIGURED");

  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = LOW;
  hw::pinLevel[Config::BOTTOM_LIMIT_SWITCH_PIN] = LOW;
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("Puck in Barrel Switch: TRIGGERED");
  ASSERT_OUT_HAS("Elevator Bottom Switch: TRIGGERED");
}
#else
void test_c_shows_unconfigured_hardware_as_not_configured(void) {
  t::run("C");
  ASSERT_OUT_HAS("Puck in Barrel Switch: NOT CONFIGURED");
  ASSERT_OUT_HAS("Elevator Bottom Switch: NOT CONFIGURED");
  ASSERT_OUT_HAS("Barrel Solenoid: NOT CONFIGURED");
  ASSERT_OUT_LACKS("OPEN");
  ASSERT_OUT_LACKS("LOCKED");
  ASSERT_OUT_LACKS("TRIGGERED");
}
#endif

}  // namespace

void run_command_tests() {
  RUN_TEST(test_question_mark_prints_every_command);
  RUN_TEST(test_help_describes_h_and_b_differently);
  RUN_TEST(test_unknown_command_prints_message_and_list);
  RUN_TEST(test_uppercase_e_is_unknown);
  RUN_TEST(test_leading_whitespace_before_command_is_ignored);
  RUN_TEST(test_empty_command_is_silent);
  RUN_TEST(test_lowercase_w_resets_servos_and_does_not_raise_elevator);
  RUN_TEST(test_uppercase_w_raises_elevator_and_not_servos);
  RUN_TEST(test_lowercase_e_arms_servos);
  RUN_TEST(test_lowercase_letters_work_for_case_insensitive_commands);
  RUN_TEST(test_lowercase_h_t_u_m_work);
  RUN_TEST(test_m_fires_both_servos_and_returns_them_to_rest);
  RUN_TEST(test_m_does_not_move_the_elevator);
  RUN_TEST(test_m_clears_the_puck_latch);
  RUN_TEST(test_m_fires_without_a_puck_present);
  RUN_TEST(test_index_argument_forms);
  RUN_TEST(test_index_out_of_range_is_rejected_without_motion);
  RUN_TEST(test_index_unparsable_is_rejected_without_motion);
  RUN_TEST(test_yaw_absolute_argument_forms);
  RUN_TEST(test_yaw_absolute_is_clamped_to_limits);
  RUN_TEST(test_yaw_relative_uses_the_tracked_angle);
  RUN_TEST(test_yaw_unparsable_values_are_rejected_without_motion);
  RUN_TEST(test_z_declares_yaw_zero_without_moving);
  RUN_TEST(test_t_reports_the_lead_driver_connection_only);
  RUN_TEST(test_t_reports_a_nonzero_spi_result);
  RUN_TEST(test_x_drives_every_step_pin_low);
  RUN_TEST(test_c_prints_axes_servos_and_sensors);
#ifdef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_x_disables_drivers_that_have_enable_pins);
  RUN_TEST(test_u_pulses_the_burnwire_for_twelve_seconds);
  RUN_TEST(test_c_shows_configured_hardware_states);
#else
  RUN_TEST(test_x_does_not_claim_drivers_are_disabled_without_enable_pins);
  RUN_TEST(test_u_reports_that_no_pulse_was_sent_without_a_trigger_pin);
  RUN_TEST(test_c_shows_unconfigured_hardware_as_not_configured);
#endif
}
