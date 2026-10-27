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
