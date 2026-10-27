// Firing servos: rest / arm / fire sequences.
#include "test_helpers.h"

namespace {

constexpr int kL = Config::LEFT_SERVO_PIN;
constexpr int kR = Config::RIGHT_SERVO_PIN;

void test_begin_attaches_both_servos_and_rests(void) {
  servoController.begin();
  EQ(2, hw::servoAttachPins.size());
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(kL));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(kR));
}

void test_arm_moves_to_the_armed_angles(void) {
  t::run("e");
  ASSERT_OUT_HAS("Both servos armed.");
  EQ(Config::LEFT_SERVO_ARM, hw::lastServoAngle(kL));
  EQ(Config::RIGHT_SERVO_ARM, hw::lastServoAngle(kR));
}

void test_arm_moves_in_small_steps(void) {
  t::run("e");
  // Rest 180 -> arm 135 in 5 degree steps passes through 175.
  TEST_ASSERT_TRUE(hw::servoWrites(kL, 175) > 0);
  TEST_ASSERT_TRUE(hw::servoWrites(kR, 5) > 0);
}

void test_fire_goes_to_fire_angles_then_rest(void) {
  t::run("m");
  ASSERT_OUT_HAS("Both servos fired.");
  const long fireIdx = hw::firstServoWrite(kL, Config::LEFT_SERVO_FIRE);
  TEST_ASSERT_TRUE(fireIdx >= 0);
  TEST_ASSERT_TRUE(hw::firstServoWrite(kR, Config::RIGHT_SERVO_FIRE) >= 0);
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(kL));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(kR));
  TEST_ASSERT_TRUE(
    hw::firstServoWrite(kL, Config::LEFT_SERVO_REST, fireIdx + 1) > fireIdx
  );
}

void test_fire_holds_for_the_configured_time(void) {
  t::run("M");
  TEST_ASSERT_TRUE(hw::totalDelayMs() >= Config::SERVO_FIRE_HOLD_MS);
}

void test_reset_returns_to_rest(void) {
  t::run("e");
  t::run("w");
  ASSERT_OUT_HAS("Both servos reset.");
  EQ(Config::LEFT_SERVO_REST, hw::lastServoAngle(kL));
  EQ(Config::RIGHT_SERVO_REST, hw::lastServoAngle(kR));
}

void test_status_reports_the_commanded_angles(void) {
  t::run("e");
  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("Left servo: 135  Right servo: 45");
}

void test_servos_never_overshoot_the_angle_range(void) {
  t::run("e");
  t::run("m");
  for (const hw::Event &e : hw::events) {
    if (e.kind == hw::Kind::ServoWrite) {
      TEST_ASSERT_TRUE(e.b >= 0 && e.b <= 180);
    }
  }
}

}  // namespace

void run_servo_tests() {
  RUN_TEST(test_begin_attaches_both_servos_and_rests);
  RUN_TEST(test_arm_moves_to_the_armed_angles);
  RUN_TEST(test_arm_moves_in_small_steps);
  RUN_TEST(test_fire_goes_to_fire_angles_then_rest);
  RUN_TEST(test_fire_holds_for_the_configured_time);
  RUN_TEST(test_reset_returns_to_rest);
  RUN_TEST(test_status_reports_the_commanded_angles);
  RUN_TEST(test_servos_never_overshoot_the_angle_range);
}
