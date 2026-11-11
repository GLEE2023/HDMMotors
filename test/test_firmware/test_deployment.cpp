// G full deployment and the fire confirmation prompt.
#include "test_helpers.h"

namespace {

constexpr long kPerPuckCycle = t::kPuckHeightSteps + 2 * t::kThreeMmSteps;
constexpr long kTotalBeforeReturn = t::kLevelOneSteps + 17 * kPerPuckCycle;

long fireCount() {
  return hw::servoWrites(Config::LEFT_SERVO_PIN, Config::LEFT_SERVO_FIRE);
}

void expectServosAtRest() {
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(Config::LEFT_SERVO_PIN));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(Config::RIGHT_SERVO_PIN));
}

// --- confirmation prompt ---------------------------------------------------------

void test_enter_fires(void) {
  Serial.feed("\n");
  TEST_ASSERT_TRUE(waitForFireConfirmation());
  ASSERT_OUT_HAS("Press Enter to fire, or type N to cancel.");
}

void test_blank_then_enter_fires(void) {
  Serial.feed("  \n");
  TEST_ASSERT_TRUE(waitForFireConfirmation());
}

void test_y_or_lowercase_y_fires(void) {
  Serial.feed("Y\n");
  TEST_ASSERT_TRUE(waitForFireConfirmation());
  t::resetFirmware();
  Serial.feed("y\n");
  TEST_ASSERT_TRUE(waitForFireConfirmation());
}

void test_anything_else_cancels(void) {
  Serial.feed("n\n");
  TEST_ASSERT_FALSE(waitForFireConfirmation());
  t::resetFirmware();
  Serial.feed("q\n");
  TEST_ASSERT_FALSE(waitForFireConfirmation());
}

void test_yaw_commands_return_to_confirmation(void) {
  Serial.feed("Y10\nR-5\nZ\nY2\n\n");
  TEST_ASSERT_TRUE(waitForFireConfirmation());
  EQ(173, t::yawNet());
  EQ(5, t::countOf("Press Enter to fire"));
  EQ(0, fireCount());
}

void test_invalid_and_oversized_yaw_do_not_fire(void) {
  Serial.feed("Yno\nR\nZbad\n" + std::string(40, 'Y') + "\nn\n");
  TEST_ASSERT_FALSE(waitForFireConfirmation());
  ASSERT_OUT_HAS("Invalid yaw angle.");
  ASSERT_OUT_HAS("Invalid relative yaw.");
  ASSERT_OUT_HAS("Invalid yaw zero command.");
  ASSERT_OUT_HAS("Command too long; discarded.");
  EQ(0, t::yawPulses());
  EQ(0, fireCount());
}

void test_nul_bytes_in_confirmation_do_not_fire_or_move_yaw(void) {
  const std::string inputs[] = {
    std::string("\0\n", 2), std::string("Y\0\n", 3), std::string("Y10\0\n", 5)
  };
  for (const std::string &input : inputs) {
    t::resetFirmware();
    Serial.feed(input);
    TEST_ASSERT_FALSE(waitForFireConfirmation());
    ASSERT_OUT_HAS("Invalid confirmation; cancelling deployment.");
    EQ(0, fireCount());
    EQ(0, t::yawPulses());
  }
}

void test_deployment_allows_partially_indexed_barrel_for_bench_work(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  t::typeAfterPulses(Config::BARREL_STEP_PIN, 100, "x");
  t::run("N");
  t::clearOut();

  Serial.feed("n\n");
  t::run("G");
  ASSERT_OUT_HAS("Deployment cancelled.");
  TEST_ASSERT_TRUE(t::leadPulses() > 0);
  t::clearOut();
  Serial.feed("n\n");
  t::run("Q");
  ASSERT_OUT_HAS("Deployment cancelled.");
  ASSERT_OUT_HAS("Barrel deployment did not complete; FMS stopped.");
  EQ(0, fireCount());
}

void test_g_yaw_before_fire(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed("Y10\nR-10\n\nn\n");
  t::run("G");
  EQ(1, fireCount());
  EQ(2 * t::kYawTenDegreeSteps, t::yawPulses());
  ASSERT_OUT_HAS("Deployment cancelled.");
}

void test_g_resets_armed_servos_if_initial_homing_is_interrupted(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(3000, [](long) { return true; });
#else
  t::run("W");
#endif
  t::run("e");
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("G");
  ASSERT_OUT_HAS("Unable to return the elevator to its bottom reference; deployment cancelled.");
  EQ(0, fireCount());
  expectServosAtRest();
}

void test_g_yaw_through_serial_line_dispatch(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed("G\nY10\n\nn\n");
  readSerialCommands();
  EQ(1, fireCount());
  EQ(t::kYawTenDegreeSteps, t::yawPulses());
  EQ(0, commandLength);
  ASSERT_OUT_HAS("Deployment cancelled.");
}

void test_q_stops_on_interrupted_index(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed(std::string(Config::PUCK_COUNT, '\n'));
  t::typeAfterPulses(Config::BARREL_STEP_PIN, 100, "x");
  t::run("Q");
  ASSERT_OUT_HAS("Barrel indexing failed; FMS stopped.");
  EQ(Config::PUCK_COUNT, fireCount());
  EQ(0, t::leadNet());
}

void test_q_cancellation_does_not_advance_another_barrel(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed("Y10\nn\n");
  t::run("q");
  EQ(0, fireCount());
  EQ(t::kYawTenDegreeSteps, t::barrelPulses());
  ASSERT_OUT_HAS("Barrel deployment did not complete; FMS stopped.");
  ASSERT_OUT_LACKS("FMS deployment complete.");
}

void test_q_deploys_all_six_barrels(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed(std::string(Config::BARREL_POSITION_COUNT * Config::PUCK_COUNT, '\n'));
  t::run("Q");
  EQ(Config::BARREL_POSITION_COUNT * Config::PUCK_COUNT, fireCount());
  EQ(Config::BARREL_POSITION_COUNT - 1, t::countOf("Barrel index movement complete."));
  EQ((Config::BARREL_POSITION_COUNT - 1) * t::kBarrelIndexSteps, t::barrelNet());
  EQ(0, t::leadNet());
  ASSERT_OUT_HAS("FMS deployment complete.");
  size_t last = 0;
  for (int index = 0; index < Config::BARREL_POSITION_COUNT; ++index) {
    for (int puck = 0; puck < Config::PUCK_COUNT; ++puck) {
      const size_t fire = t::out().find("Firing servos...", last);
      TEST_ASSERT_TRUE(fire != std::string::npos);
      const size_t nextIndex = t::out().find("Moving barrel to index ", last);
      TEST_ASSERT_TRUE(nextIndex == std::string::npos || fire < nextIndex);
      last = fire + 1;
    }
    if (index + 1 < Config::BARREL_POSITION_COUNT) {
      const std::string target = "Moving barrel to index " + std::to_string(index + 1);
      const size_t found = t::out().find(target, last);
      TEST_ASSERT_TRUE(found != std::string::npos);
      const size_t nextFire = t::out().find("Firing servos...", last);
      TEST_ASSERT_TRUE(nextFire == std::string::npos || found < nextFire);
      last = found + target.size();
    }
  }
  TEST_ASSERT_TRUE(t::out().find("Moving barrel to index ", last) == std::string::npos);
}

void test_q_starts_with_currently_aligned_barrel(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  t::run("I3");
  t::clearOut();
  Serial.feed(std::string(Config::PUCK_COUNT, '\n') + "n\n");
  t::run("Q");
  EQ(Config::PUCK_COUNT, fireCount());
  const size_t firstFire = t::out().find("Firing servos...");
  const size_t nextIndex = t::out().find("Moving barrel to index 4");
  TEST_ASSERT_TRUE(firstFire != std::string::npos && nextIndex != std::string::npos);
  TEST_ASSERT_TRUE(firstFire < nextIndex);
  EQ(1, t::countOf("Moving barrel to index "));
}

void test_q_stops_if_final_return_is_interrupted(void) {
#ifdef HDM_TEST_PINS_CONFIGURED
  t::simulateLeadScrew(0, [](long) { return true; });
#endif
  Serial.feed(std::string(Config::PUCK_COUNT, '\n'));
  t::typeAfterPulses(Config::LEAD_STEP_PIN, kTotalBeforeReturn + 100, "x");
  t::run("Q");
  EQ(Config::PUCK_COUNT, fireCount());
  EQ(0, t::countOf("Barrel index movement complete."));
  ASSERT_OUT_HAS("did not reach the bottom reference");
  ASSERT_OUT_HAS("Barrel deployment did not complete; FMS stopped.");
}

#ifdef HDM_TEST_PINS_CONFIGURED
void test_q_stops_if_initial_homing_is_interrupted(void) {
  t::simulateLeadScrew(3000, [](long) { return true; });
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("Q");
  EQ(0, t::barrelPulses());
  EQ(0, fireCount());
  ASSERT_OUT_HAS("Unable to return the elevator to its bottom reference; deployment cancelled.");
  ASSERT_OUT_HAS("Barrel deployment did not complete; FMS stopped.");
}
#endif

// --- G, no puck switch ------------------------------------------------------------

#ifndef HDM_TEST_PINS_CONFIGURED

void test_g_fires_every_puck_and_returns_to_the_bottom(void) {
  Serial.feed(std::string(17, '\n'));
  t::run("G");
  EQ(17, fireCount());
  ASSERT_OUT_HAS("Puck switch is not configured; puck presence will not be verified.");
  EQ(1, t::countOf("Puck switch is not configured"));
  ASSERT_OUT_LACKS("full deployment stopped");
  ASSERT_OUT_LACKS("did not reach the bottom reference");
  EQ(0, t::leadNet());
  EQ(kTotalBeforeReturn + t::kLevelOneSteps + 17 * t::kPuckHeightSteps,
     t::leadPulses());
  EQ(0, axes.getLeadPuckLevel());
}

void test_g_sequence_for_the_first_puck(void) {
  Serial.feed("n\n");
  t::run("G");
  // Raise to level 1, then one puck up, then lower 3 mm before the prompt.
  // Pulses: up 1310, up 1411, down 302, then H returns the 2419 still tracked.
  const long atPrompt = t::kLevelOneSteps + t::kPuckHeightSteps - t::kThreeMmSteps;
  EQ(t::kLevelOneSteps + t::kPuckHeightSteps + t::kThreeMmSteps + atPrompt,
     t::leadPulses());
  ASSERT_OUT_HAS("Deployment cancelled.");
  EQ(0, fireCount());
  EQ(0, t::leadNet());
}

void test_g_cancel_returns_the_elevator_to_the_bottom(void) {
  Serial.feed("n\n");
  t::run("G");
  ASSERT_OUT_HAS("Deployment cancelled.");
  EQ(0, t::leadNet());
}

void test_g_can_cancel_after_some_pucks(void) {
  Serial.feed(std::string(3, '\n') + "n\n");
  t::run("G");
  EQ(3, fireCount());
  ASSERT_OUT_HAS("Deployment cancelled.");
  EQ(0, t::leadNet());
}

void test_g_stop_during_the_first_raise_aborts(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("G");
  ASSERT_OUT_HAS("Unable to raise to the first puck level; full deployment stopped.");
  EQ(0, fireCount());
  EQ(0, hw::servoWrites(Config::LEFT_SERVO_PIN, Config::LEFT_SERVO_ARM));
}

void test_g_stop_while_raising_the_puck_aborts(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, t::kLevelOneSteps + 100, "x");
  t::run("G");
  ASSERT_OUT_HAS("Unable to raise the puck to firing height; full deployment stopped.");
  EQ(0, fireCount());
  EQ(0, hw::servoWrites(Config::LEFT_SERVO_PIN, Config::LEFT_SERVO_ARM));
}

void test_g_stop_during_the_pre_fire_lower_aborts(void) {
  t::typeAfterPulses(
    Config::LEAD_STEP_PIN, t::kLevelOneSteps + t::kPuckHeightSteps + 100, "x"
  );
  t::run("G");
  ASSERT_OUT_HAS("Unable to position the puck for firing; full deployment stopped.");
  EQ(0, fireCount());
  expectServosAtRest();
}

void test_g_stop_during_the_post_fire_raise_aborts_after_firing(void) {
  Serial.feed("\n");
  t::typeAfterPulses(
    Config::LEAD_STEP_PIN,
    t::kLevelOneSteps + t::kPuckHeightSteps + t::kThreeMmSteps + 100,
    "x"
  );
  t::run("G");
  ASSERT_OUT_HAS("Unable to finish the puck cycle; full deployment stopped.");
  EQ(1, fireCount());
  expectServosAtRest();
}

void test_g_cancel_resets_the_servos(void) {
  Serial.feed("n\n");
  t::run("G");
  TEST_ASSERT_TRUE(hw::servoWrites(Config::LEFT_SERVO_PIN, Config::LEFT_SERVO_ARM) > 0);
  expectServosAtRest();
}

#else

void test_g_fires_every_puck_when_all_are_present(void) {
  t::simulateLeadScrew(0, [](long) { return true; });
  Serial.feed(std::string(17, '\n'));
  t::run("G");
  EQ(17, fireCount());
  ASSERT_OUT_LACKS("not configured; puck presence");
  ASSERT_OUT_LACKS("skipping deployment");
}

void test_g_skips_levels_with_no_puck(void) {
  t::simulateLeadScrew(0, [](long) { return false; });
  t::run("G");
  EQ(0, fireCount());
  EQ(17, t::countOf("No puck detected for level"));
  ASSERT_OUT_LACKS("Firing servos...");
}

void test_g_fires_only_when_the_puck_switch_trips_briefly(void) {
  t::simulateLeadScrew(0, [](long steps) { return steps >= 2000 && steps < 2100; });
  Serial.feed(std::string(17, '\n'));
  t::run("G");
  EQ(1, fireCount());
}

void test_g_homes_with_the_bottom_switch_before_the_first_puck(void) {
  // The elevator starts physically 3000 steps up while the software says 0.
  t::simulateLeadScrew(3000, [](long) { return true; });
  Serial.feed(std::string(17, '\n'));
  t::run("G");
  EQ(17, fireCount());
  EQ(-3000, t::leadNet());
  const long firstUp = hw::firstWrite(Config::LEAD_DIRECTION_PIN, HIGH);
  TEST_ASSERT_TRUE(firstUp > hw::firstWrite(Config::LEAD_STEP_PIN, HIGH));
}

void test_g_cancel_with_a_switch_returns_to_the_physical_bottom(void) {
  t::simulateLeadScrew(0, [](long) { return true; });
  Serial.feed("n\n");
  t::run("G");
  ASSERT_OUT_HAS("Deployment cancelled.");
  EQ(0, t::leadNet());
  expectServosAtRest();
}

#endif

}  // namespace

void run_deployment_tests() {
  RUN_TEST(test_enter_fires);
  RUN_TEST(test_blank_then_enter_fires);
  RUN_TEST(test_y_or_lowercase_y_fires);
  RUN_TEST(test_anything_else_cancels);
  RUN_TEST(test_yaw_commands_return_to_confirmation);
  RUN_TEST(test_invalid_and_oversized_yaw_do_not_fire);
  RUN_TEST(test_nul_bytes_in_confirmation_do_not_fire_or_move_yaw);
  RUN_TEST(test_deployment_allows_partially_indexed_barrel_for_bench_work);
  RUN_TEST(test_g_yaw_before_fire);
  RUN_TEST(test_g_resets_armed_servos_if_initial_homing_is_interrupted);
  RUN_TEST(test_g_yaw_through_serial_line_dispatch);
  RUN_TEST(test_q_stops_on_interrupted_index);
  RUN_TEST(test_q_cancellation_does_not_advance_another_barrel);
  RUN_TEST(test_q_deploys_all_six_barrels);
  RUN_TEST(test_q_starts_with_currently_aligned_barrel);
  RUN_TEST(test_q_stops_if_final_return_is_interrupted);
#ifdef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_q_stops_if_initial_homing_is_interrupted);
#endif
#ifndef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_g_fires_every_puck_and_returns_to_the_bottom);
  RUN_TEST(test_g_sequence_for_the_first_puck);
  RUN_TEST(test_g_cancel_returns_the_elevator_to_the_bottom);
  RUN_TEST(test_g_can_cancel_after_some_pucks);
  RUN_TEST(test_g_stop_during_the_first_raise_aborts);
  RUN_TEST(test_g_stop_while_raising_the_puck_aborts);
  RUN_TEST(test_g_stop_during_the_pre_fire_lower_aborts);
  RUN_TEST(test_g_stop_during_the_post_fire_raise_aborts_after_firing);
  RUN_TEST(test_g_cancel_resets_the_servos);
#else
  RUN_TEST(test_g_fires_every_puck_when_all_are_present);
  RUN_TEST(test_g_skips_levels_with_no_puck);
  RUN_TEST(test_g_fires_only_when_the_puck_switch_trips_briefly);
  RUN_TEST(test_g_homes_with_the_bottom_switch_before_the_first_puck);
  RUN_TEST(test_g_cancel_with_a_switch_returns_to_the_physical_bottom);
#endif
}
