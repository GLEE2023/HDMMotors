// Barrel indexing (N/P/I) and yaw (Y/R/Z) behavior, including the shared
// yaw + barrel pulses.
#include "test_helpers.h"

namespace {

// --- barrel -----------------------------------------------------------------------

void test_n_advances_one_index(void) {
  t::run("N");
  EQ(t::kBarrelIndexSteps, t::barrelNet());
  EQ(0, t::yawPulses());
  EQ(0, t::leadPulses());
  ASSERT_OUT_HAS("Moving barrel to index 1");
  ASSERT_OUT_HAS("Barrel index movement complete.");
}

void test_p_wraps_back_from_index_zero(void) {
  t::run("P");
  EQ(-t::kBarrelIndexSteps, t::barrelNet());
  ASSERT_OUT_HAS("Moving barrel to index 5");
}

void test_six_n_commands_make_one_revolution(void) {
  for (int i = 0; i < 6; ++i) {
    t::run("N");
  }
  EQ(t::kBarrelRevolutionSteps, t::barrelNet());
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("Barrel index: 0 of 6");
}

void test_i_takes_the_shortest_direction(void) {
  t::run("I3");  // exactly half a turn goes forward
  EQ(4440, t::barrelNet());

  t::resetFirmware();
  t::run("I4");
  EQ(-2960, t::barrelNet());

  t::resetFirmware();
  t::run("I5");
  EQ(-t::kBarrelIndexSteps, t::barrelNet());
}

void test_i_to_the_current_index_does_not_move(void) {
  t::run("I0");
  EQ(0, t::barrelPulses());
}

void test_i_rejects_an_out_of_range_index(void) {
  t::run("I6");
  ASSERT_OUT_HAS("Barrel index must be between 0 and 5.");
  EQ(0, t::barrelPulses());
}

void test_barrel_move_leaves_yaw_alone(void) {
  t::run("N");
  EQ(0, t::yawPulses());
}

void test_stopped_barrel_move_reports_and_keeps_partial_position(void) {
  t::typeAfterPulses(Config::BARREL_STEP_PIN, 100, "x");
  t::run("N");
  ASSERT_OUT_HAS("Barrel movement stopped before completion.");
  EQ(112, t::barrelNet());
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("Barrel position: 112 / 8880");
}

#ifdef HDM_TEST_PINS_CONFIGURED

void test_solenoid_is_unlocked_for_the_move_and_locked_after(void) {
  t::run("N");
  const long unlock = hw::firstWrite(Config::BARREL_SOLENOID_PIN, HIGH);
  const long lock = hw::lastWrite(Config::BARREL_SOLENOID_PIN, LOW);
  TEST_ASSERT_TRUE(unlock >= 0);
  TEST_ASSERT_TRUE(lock > unlock);
  const long firstStep = hw::firstWrite(Config::BARREL_STEP_PIN, HIGH);
  TEST_ASSERT_TRUE(unlock < firstStep);
  TEST_ASSERT_TRUE(lock > hw::lastWrite(Config::BARREL_STEP_PIN, HIGH));
  ASSERT_OUT_LACKS("Barrel solenoid is not configured");
}

void test_solenoid_is_relocked_after_an_emergency_stop(void) {
  t::typeAfterPulses(Config::BARREL_STEP_PIN, 100, "x");
  t::run("N");
  const long lock = hw::lastWrite(Config::BARREL_SOLENOID_PIN, LOW);
  TEST_ASSERT_TRUE(lock > hw::lastWrite(Config::BARREL_STEP_PIN, HIGH));
}

#else

void test_indexing_works_without_a_solenoid_and_says_so(void) {
  t::run("N");
  ASSERT_OUT_HAS("Barrel solenoid is not configured; indexing without unlocking.");
  EQ(t::kBarrelIndexSteps, t::barrelNet());
  TEST_ASSERT_FALSE(hw::wroteTo(Config::UNUSED_PIN));
}

#endif

// --- yaw ----------------------------------------------------------------------------

void test_yaw_moves_the_yaw_and_barrel_motors_together(void) {
  t::run("Y10");
  EQ(t::kYawTenDegreeSteps, t::yawNet());
  EQ(t::kYawTenDegreeSteps, t::barrelNet());
  ASSERT_OUT_HAS("Yaw movement complete.");
}

void test_yaw_does_not_change_the_tracked_barrel_position(void) {
  t::run("Y10");
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("Barrel position: 0 / 8880");
  ASSERT_OUT_HAS("Yaw position: 247 steps");
}

void test_yaw_motors_share_pulses_not_just_direction(void) {
  t::run("Y-10");
  EQ(-t::kYawTenDegreeSteps, t::yawNet());
  EQ(-t::kYawTenDegreeSteps, t::barrelNet());
  EQ(t::yawPulses(), t::barrelPulses());
}

void test_yaw_relative_is_cumulative_and_rounds_steps(void) {
  t::run("R-0.25");
  EQ(-6, t::yawNet());

  t::resetFirmware();
  t::run("R5");
  t::run("R5");
  EQ(246, t::yawNet());
}

void test_yaw_clamps_to_the_limits(void) {
  t::run("Y45");
  EQ(t::kYawLimitSteps, t::yawNet());
  ASSERT_OUT_HAS("outside the yaw limits; clamped.");

  t::run("Y-45");
  EQ(-t::kYawLimitSteps, t::yawNet());
}

void test_yaw_beyond_limit_does_not_move_further(void) {
  t::run("Y30");
  const long before = t::yawPulses();
  t::run("R5");
  EQ(before, t::yawPulses());
}

void test_z_zeroes_yaw_without_moving(void) {
  t::run("Y10");
  const long before = t::yawPulses();
  t::run("Z");
  ASSERT_OUT_HAS("Current yaw position set as 0 degrees.");
  EQ(before, t::yawPulses());

  t::run("Y0");
  EQ(before, t::yawPulses());
  t::run("Y5");
  EQ(before + 123, t::yawPulses());
}

void test_stopped_yaw_move_reports(void) {
  t::typeAfterPulses(Config::YAW_STEP_PIN, 100, "x");
  t::run("Y30");
  ASSERT_OUT_HAS("Yaw movement stopped before completion.");
  EQ(112, t::yawNet());
}

}  // namespace

void run_barrel_yaw_tests() {
  RUN_TEST(test_n_advances_one_index);
  RUN_TEST(test_p_wraps_back_from_index_zero);
  RUN_TEST(test_six_n_commands_make_one_revolution);
  RUN_TEST(test_i_takes_the_shortest_direction);
  RUN_TEST(test_i_to_the_current_index_does_not_move);
  RUN_TEST(test_i_rejects_an_out_of_range_index);
  RUN_TEST(test_barrel_move_leaves_yaw_alone);
  RUN_TEST(test_stopped_barrel_move_reports_and_keeps_partial_position);
#ifdef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_solenoid_is_unlocked_for_the_move_and_locked_after);
  RUN_TEST(test_solenoid_is_relocked_after_an_emergency_stop);
#else
  RUN_TEST(test_indexing_works_without_a_solenoid_and_says_so);
#endif
  RUN_TEST(test_yaw_moves_the_yaw_and_barrel_motors_together);
  RUN_TEST(test_yaw_does_not_change_the_tracked_barrel_position);
  RUN_TEST(test_yaw_motors_share_pulses_not_just_direction);
  RUN_TEST(test_yaw_relative_is_cumulative_and_rounds_steps);
  RUN_TEST(test_yaw_clamps_to_the_limits);
  RUN_TEST(test_yaw_beyond_limit_does_not_move_further);
  RUN_TEST(test_z_zeroes_yaw_without_moving);
  RUN_TEST(test_stopped_yaw_move_reports);
}
