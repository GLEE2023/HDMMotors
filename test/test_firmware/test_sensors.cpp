// Sensors: switches, puck latch, solenoid, burnwire; unconfigured pins stay untouched.
#include "test_helpers.h"

namespace {

void test_configuration_flags_match_config(void) {
  TEST_ASSERT_EQUAL(t::kBottomSwitch, sensors.hasBottomSwitch());
  TEST_ASSERT_EQUAL(t::kPuckSwitch, sensors.hasPuckSwitch());
  TEST_ASSERT_EQUAL(t::kSolenoid, sensors.hasBarrelSolenoid());
  TEST_ASSERT_EQUAL(t::kBurnwire, sensors.hasBurnwireTrigger());
}

void test_unconfigured_sensors_never_touch_pin_255(void) {
  sensors.begin();
  sensors.updatePuckLatch();
  sensors.unlockBarrel();
  sensors.lockBarrel();
  sensors.fireBurnwire(10);
  (void)sensors.isLeadBottomSwitchTripped();
  (void)sensors.isLeadTopSwitchTripped();
  TEST_ASSERT_FALSE(hw::wroteTo(Config::UNUSED_PIN));
  EQ(-1, hw::pinModeOf[Config::UNUSED_PIN]);
}

void test_status_prints_every_sensor(void) {
  t::run("C");
  ASSERT_OUT_HAS("Puck in Barrel Switch:");
  ASSERT_OUT_HAS("Elevator Bottom Switch:");
  ASSERT_OUT_HAS("Barrel Solenoid:");
}

#ifdef HDM_TEST_PINS_CONFIGURED

void test_switches_are_active_low(void) {
  hw::pinLevel[Config::BOTTOM_LIMIT_SWITCH_PIN] = HIGH;
  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = HIGH;
  TEST_ASSERT_FALSE(sensors.isLeadBottomSwitchTripped());
  TEST_ASSERT_FALSE(sensors.isLeadTopSwitchTripped());

  hw::pinLevel[Config::BOTTOM_LIMIT_SWITCH_PIN] = LOW;
  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = LOW;
  TEST_ASSERT_TRUE(sensors.isLeadBottomSwitchTripped());
  TEST_ASSERT_TRUE(sensors.isLeadTopSwitchTripped());
}

void test_puck_latch_stays_set_until_cleared(void) {
  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = HIGH;
  sensors.updatePuckLatch();
  TEST_ASSERT_FALSE(sensors.isPuckLatched());

  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = LOW;
  sensors.updatePuckLatch();
  TEST_ASSERT_TRUE(sensors.isPuckLatched());

  hw::pinLevel[Config::TOP_LIMIT_SWITCH_PIN] = HIGH;
  sensors.updatePuckLatch();
  TEST_ASSERT_TRUE(sensors.isPuckLatched());

  sensors.clearPuckLatch();
  TEST_ASSERT_FALSE(sensors.isPuckLatched());
}

void test_begin_locks_the_barrel_and_idles_the_burnwire(void) {
  sensors.begin();
  EQ(LOW, hw::pinLevel[Config::BARREL_SOLENOID_PIN]);
  EQ(LOW, hw::pinLevel[Config::BURNWIRE_TRIGGER_PIN]);
}

void test_solenoid_settle_delay_only_on_state_change(void) {
  sensors.begin();
  hw::events.clear();
  sensors.lockBarrel();  // already locked
  EQ(0, hw::totalDelayMs());

  sensors.unlockBarrel();
  EQ(HIGH, hw::pinLevel[Config::BARREL_SOLENOID_PIN]);
  EQ(Config::BARREL_SOLENOID_SETTLE_TIME_MS, hw::totalDelayMs());

  sensors.unlockBarrel();  // already unlocked
  EQ(Config::BARREL_SOLENOID_SETTLE_TIME_MS, hw::totalDelayMs());

  sensors.lockBarrel();
  EQ(LOW, hw::pinLevel[Config::BARREL_SOLENOID_PIN]);
  EQ(2 * Config::BARREL_SOLENOID_SETTLE_TIME_MS, hw::totalDelayMs());
}

void test_burnwire_pulses_high_for_the_duration(void) {
  sensors.begin();
  hw::events.clear();
  sensors.fireBurnwire(500);
  const long on = hw::firstWrite(Config::BURNWIRE_TRIGGER_PIN, HIGH);
  const long off = hw::lastWrite(Config::BURNWIRE_TRIGGER_PIN, LOW);
  TEST_ASSERT_TRUE(on >= 0);
  TEST_ASSERT_TRUE(off > on);
  EQ(500, hw::totalDelayMs());
}

void test_u_command_pulses_the_burnwire_for_twelve_seconds(void) {
  t::run("U");
  ASSERT_OUT_HAS("Burnwire pulse started.");
  ASSERT_OUT_HAS("Burnwire pulse complete.");
  EQ(t::kBurnwireMs, hw::totalDelayMs());
  EQ(LOW, hw::pinLevel[Config::BURNWIRE_TRIGGER_PIN]);
}

#else

void test_u_command_reports_unconfigured_burnwire(void) {
  t::run("U");
  ASSERT_OUT_HAS("Burnwire trigger pin is not configured; no pulse sent.");
  EQ(0, hw::totalDelayMs());
}

void test_unconfigured_switches_never_trip(void) {
  TEST_ASSERT_FALSE(sensors.isLeadBottomSwitchTripped());
  TEST_ASSERT_FALSE(sensors.isLeadTopSwitchTripped());
  sensors.updatePuckLatch();
  TEST_ASSERT_FALSE(sensors.isPuckLatched());
}

#endif

}  // namespace

void run_sensor_tests() {
  RUN_TEST(test_configuration_flags_match_config);
  RUN_TEST(test_unconfigured_sensors_never_touch_pin_255);
  RUN_TEST(test_status_prints_every_sensor);
#ifdef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_switches_are_active_low);
  RUN_TEST(test_puck_latch_stays_set_until_cleared);
  RUN_TEST(test_begin_locks_the_barrel_and_idles_the_burnwire);
  RUN_TEST(test_solenoid_settle_delay_only_on_state_change);
  RUN_TEST(test_burnwire_pulses_high_for_the_duration);
  RUN_TEST(test_u_command_pulses_the_burnwire_for_twelve_seconds);
#else
  RUN_TEST(test_u_command_reports_unconfigured_burnwire);
  RUN_TEST(test_unconfigured_switches_never_trip);
#endif
}
