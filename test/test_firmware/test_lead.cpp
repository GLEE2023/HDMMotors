// Lead screw: W/S puck levels, +/- jogs, and the deliberately different H and B.
#include "test_helpers.h"

namespace {

// --- W / S --------------------------------------------------------------------

void test_w_raises_first_puck_by_the_extra_offset(void) {
  t::run("W");
  EQ(t::kLevelOneSteps, t::leadNet());
  EQ(Config::LEAD_POSITIVE_DIRECTION_LEVEL, hw::pinLevel[Config::LEAD_DIRECTION_PIN]);
  ASSERT_OUT_HAS("(1310 steps)");
  ASSERT_OUT_HAS("Lead puck level: 1 / 17");
}

void test_w_raises_later_pucks_by_one_puck_height(void) {
  t::run("W");
  t::run("W");
  EQ(t::kLevelTwoSteps, t::leadNet());
  ASSERT_OUT_HAS("Lead puck level: 2 / 17");

  t::run("W");
  EQ(4133, t::leadNet());  // 13 + 2 * 14 = 41 mm
  ASSERT_OUT_HAS("Lead puck level: 3 / 17");
}

void test_w_stops_at_the_last_puck_level(void) {
  for (int i = 0; i < 17; ++i) {
    t::run("W");
  }
  EQ(23890, t::leadNet());  // 13 + 16 * 14 = 237 mm
  ASSERT_OUT_HAS("Lead puck level: 17 / 17");

  const long pulsesBefore = t::leadPulses();
  t::clearOut();
  t::run("W");
  ASSERT_OUT_HAS("All configured pucks have already been raised.");
  EQ(pulsesBefore, t::leadPulses());
}

void test_s_lowers_one_puck_level(void) {
  t::run("W");
  t::run("W");
  t::run("S");
  EQ(t::kLevelOneSteps, t::leadNet());
  EQ(LOW, hw::pinLevel[Config::LEAD_DIRECTION_PIN]);
  ASSERT_OUT_HAS("Lead puck level: 1 / 17");

  t::run("S");
  EQ(0, t::leadNet());
  ASSERT_OUT_HAS("Lead puck level: 0 / 17");
}

void test_s_at_the_bottom_level_does_nothing(void) {
  t::run("S");
  ASSERT_OUT_HAS("Lead screw is already at the bottom puck level.");
  EQ(0, t::leadPulses());
}

void test_move_to_puck_level_validates_the_level(void) {
  TEST_ASSERT_FALSE(axes.moveLeadToPuckLevel(18));
  ASSERT_OUT_HAS("All configured pucks have already been raised.");
  EQ(0, t::leadPulses());

  TEST_ASSERT_TRUE(axes.moveLeadToPuckLevel(2));
  TEST_ASSERT_TRUE(axes.moveLeadToPuckLevel(0));
  EQ(0, t::leadNet());
  EQ(0, axes.getLeadPuckLevel());
}

// --- + / - jogs -----------------------------------------------------------------

void test_plus_jogs_up_one_millimeter(void) {
  t::run("+");
  EQ(t::kOneMmSteps, t::leadNet());
  ASSERT_OUT_HAS("(101 steps)");
}

void test_minus_jogs_down_one_millimeter(void) {
  t::run("+");
  t::run("+");
  t::run("-");
  EQ(t::kOneMmSteps, t::leadNet());
}

void test_minus_at_the_bottom_is_clamped_and_keeps_level_zero(void) {
  t::run("-");
  ASSERT_OUT_HAS("Lead screw is already at that position.");
  EQ(0, t::leadPulses());
  EQ(0, axes.getLeadPuckLevel());
}

void test_jog_that_crosses_a_puck_height_updates_the_level(void) {
  for (int i = 0; i < 13; ++i) {
    t::run("+");
  }
  EQ(1, axes.getLeadPuckLevel());  // 13 jogs = 1313 steps, past the 1310-step level 1
}

void test_clamped_jog_at_the_top_retains_the_clamped_reference(void) {
  TEST_ASSERT_FALSE(axes.moveLeadRelativeMillimeters(300.0f));
  EQ(t::kFullTravelSteps, t::leadNet());
  EQ(17, axes.getLeadPuckLevel());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 256.005f, axes.getLeadPositionMillimeters());

  const long pulsesBefore = t::leadPulses();
  t::run("+");
  EQ(pulsesBefore, t::leadPulses());

  t::run("-");
  EQ(t::kFullTravelSteps - t::kOneMmSteps, t::leadNet());
}

void test_relative_move_return_values(void) {
  TEST_ASSERT_TRUE(axes.moveLeadRelativeMillimeters(0.0f));
  EQ(0, t::leadPulses());

  TEST_ASSERT_TRUE(axes.moveLeadRelativeMillimeters(14.0f));
  EQ(t::kPuckHeightSteps, t::leadNet());

  TEST_ASSERT_FALSE(axes.moveLeadRelativeMillimeters(-500.0f));
  EQ(0, t::leadNet());
}

// --- unknown level after a stopped move -------------------------------------------

void test_stopped_jog_marks_level_unknown_but_keeps_position(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 20, "x");
  t::run("+");
  ASSERT_OUT_HAS("EMERGENCY STOP");
  ASSERT_OUT_HAS("Lead-screw movement stopped before completion.");
  EQ(32, t::leadNet());
  EQ(-1, axes.getLeadPuckLevel());

  t::clearOut();
  t::run("C");
  ASSERT_OUT_HAS("(32 steps)");
  ASSERT_OUT_HAS("Lead puck level: unknown");
}

void test_w_infers_the_level_from_position_when_unknown(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 20, "x");
  t::run("+");
  EQ(-1, axes.getLeadPuckLevel());

  t::run("W");
  EQ(t::kLevelOneSteps, t::leadNet());
  EQ(1, axes.getLeadPuckLevel());
}

void test_s_infers_the_level_from_position_when_unknown(void) {
  t::run("W");
  t::run("W");

  t::typeAfterPulses(Config::LEAD_STEP_PIN, 20, "x");
  t::run("+");
  EQ(-1, axes.getLeadPuckLevel());

  t::run("S");
  EQ(t::kLevelOneSteps, t::leadNet());
  EQ(1, axes.getLeadPuckLevel());
}

void test_stopped_w_keeps_the_partial_position_for_h(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("W");
  EQ(112, t::leadNet());
  EQ(-1, axes.getLeadPuckLevel());
  ASSERT_OUT_HAS("Lead-screw movement stopped before completion.");
}

// --- B: full-travel descent that ignores the tracked position ----------------------

void test_b_ignores_a_tracked_position_of_zero(void) {
  t::run("B");
  ASSERT_OUT_HAS("Running lead screw down for the full configured travel...");
  EQ(t::kFullTravelSteps, t::leadPulses());
  EQ(-t::kFullTravelSteps, t::leadNet());
  EQ(0, axes.getLeadPuckLevel());
}

void test_b_runs_the_full_travel_even_when_tracked_position_is_higher(void) {
  t::run("W");
  t::run("W");
  t::run("W");
  const long before = t::leadPulses();
  t::run("B");
  EQ(t::kFullTravelSteps, t::leadPulses() - before);
  EQ(0, axes.getLeadPuckLevel());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, axes.getLeadPositionMillimeters());
}

void test_b_stopped_by_x_zeroes_the_position(void) {
  t::run("W");
  t::clearOut();
  const long before = t::leadPulses();
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");

  t::run("B");
  ASSERT_OUT_HAS("EMERGENCY STOP");
  ASSERT_OUT_HAS("Lead screw stopped early; current position set as bottom (0 mm).");
  EQ(112, t::leadPulses() - before);
  EQ(0, axes.getLeadPuckLevel());
  TEST_ASSERT_EQUAL_FLOAT(0.0f, axes.getLeadPositionMillimeters());

  // The stop point is now the reference, so a follow-up W starts from "bottom".
  const long afterStop = t::leadPulses();
  t::run("W");
  EQ(t::kLevelOneSteps, t::leadPulses() - afterStop);
}

#ifdef HDM_TEST_PINS_CONFIGURED

void test_b_stops_early_when_the_bottom_switch_trips(void) {
  t::simulateLeadScrew(5000);
  t::run("B");
  EQ(5000, t::leadPulses());
  EQ(-5000, t::leadNet());
  ASSERT_OUT_HAS("Bottom microswitch tripped; lead screw stopped.");
  ASSERT_OUT_HAS("Lead screw reached the physical bottom stop.");
  EQ(0, axes.getLeadPuckLevel());
}

void test_b_stops_when_the_switch_trips_exactly_on_a_chunk_boundary(void) {
  t::simulateLeadScrew(4000);
  t::run("B");
  EQ(4000, t::leadPulses());
  ASSERT_OUT_HAS("Lead screw reached the physical bottom stop.");
}

void test_b_does_nothing_when_the_switch_is_already_tripped(void) {
  t::simulateLeadScrew(0);
  t::run("B");
  EQ(0, t::leadPulses());
  ASSERT_OUT_HAS("Lead screw reached the physical bottom stop.");
}

void test_b_assumes_the_bottom_when_the_switch_never_trips(void) {
  t::run("B");
  EQ(t::kFullTravelSteps, t::leadPulses());
  ASSERT_OUT_HAS("Bottom switch did not trip; assuming bottom from the step count.");
}

void test_h_uses_the_bottom_switch_not_the_tracked_position(void) {
  // The software position says 0 but the elevator is physically higher.
  t::simulateLeadScrew(3000);
  t::run("H");
  EQ(3000, t::leadPulses());
  ASSERT_OUT_LACKS("not configured");
  EQ(0, axes.getLeadPuckLevel());
}

void test_b_stopped_by_x_with_a_switch_still_zeroes_the_position(void) {
  t::simulateLeadScrew(20000);
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("B");
  EQ(112, t::leadPulses());
  ASSERT_OUT_HAS("Lead screw stopped early; current position set as bottom (0 mm).");
  EQ(0, axes.getLeadPuckLevel());
}

#else

void test_b_assumes_the_bottom_after_a_full_travel_run(void) {
  t::run("B");
  ASSERT_OUT_HAS("Bottom switch did not trip; assuming bottom from the step count.");
  ASSERT_OUT_LACKS("reached the physical bottom stop");
}

// --- H: software bottom reference --------------------------------------------------

void test_h_without_a_switch_uses_the_software_position(void) {
  t::run("H");
  ASSERT_OUT_HAS("Bottom limit switch is not configured; using the software bottom reference.");
  ASSERT_OUT_HAS("Lead screw is already at that position.");
  EQ(0, t::leadPulses());
}

void test_h_moves_down_exactly_the_tracked_distance(void) {
  t::run("W");
  t::run("W");
  const long before = t::leadPulses();
  t::run("H");
  EQ(t::kLevelTwoSteps, t::leadPulses() - before);
  EQ(0, t::leadNet());
  EQ(0, axes.getLeadPuckLevel());
}

void test_h_and_b_are_not_interchangeable(void) {
  t::run("H");
  EQ(0, t::leadPulses());

  t::run("B");
  EQ(t::kFullTravelSteps, t::leadPulses());
}

void test_h_after_a_stopped_move_returns_the_partial_distance(void) {
  t::typeAfterPulses(Config::LEAD_STEP_PIN, 100, "x");
  t::run("W");
  const long before = t::leadPulses();
  t::run("H");
  EQ(112, t::leadPulses() - before);
  EQ(0, t::leadNet());
  EQ(0, axes.getLeadPuckLevel());
}

#endif

}  // namespace

void run_lead_tests() {
  RUN_TEST(test_w_raises_first_puck_by_the_extra_offset);
  RUN_TEST(test_w_raises_later_pucks_by_one_puck_height);
  RUN_TEST(test_w_stops_at_the_last_puck_level);
  RUN_TEST(test_s_lowers_one_puck_level);
  RUN_TEST(test_s_at_the_bottom_level_does_nothing);
  RUN_TEST(test_move_to_puck_level_validates_the_level);
  RUN_TEST(test_plus_jogs_up_one_millimeter);
  RUN_TEST(test_minus_jogs_down_one_millimeter);
  RUN_TEST(test_minus_at_the_bottom_is_clamped_and_keeps_level_zero);
  RUN_TEST(test_jog_that_crosses_a_puck_height_updates_the_level);
  RUN_TEST(test_clamped_jog_at_the_top_retains_the_clamped_reference);
  RUN_TEST(test_relative_move_return_values);
  RUN_TEST(test_stopped_jog_marks_level_unknown_but_keeps_position);
  RUN_TEST(test_w_infers_the_level_from_position_when_unknown);
  RUN_TEST(test_s_infers_the_level_from_position_when_unknown);
  RUN_TEST(test_stopped_w_keeps_the_partial_position_for_h);
  RUN_TEST(test_b_ignores_a_tracked_position_of_zero);
  RUN_TEST(test_b_runs_the_full_travel_even_when_tracked_position_is_higher);
  RUN_TEST(test_b_stopped_by_x_zeroes_the_position);
#ifdef HDM_TEST_PINS_CONFIGURED
  RUN_TEST(test_b_stops_early_when_the_bottom_switch_trips);
  RUN_TEST(test_b_stops_when_the_switch_trips_exactly_on_a_chunk_boundary);
  RUN_TEST(test_b_does_nothing_when_the_switch_is_already_tripped);
  RUN_TEST(test_b_assumes_the_bottom_when_the_switch_never_trips);
  RUN_TEST(test_h_uses_the_bottom_switch_not_the_tracked_position);
  RUN_TEST(test_b_stopped_by_x_with_a_switch_still_zeroes_the_position);
#else
  RUN_TEST(test_b_assumes_the_bottom_after_a_full_travel_run);
  RUN_TEST(test_h_without_a_switch_uses_the_software_position);
  RUN_TEST(test_h_moves_down_exactly_the_tracked_distance);
  RUN_TEST(test_h_and_b_are_not_interchangeable);
  RUN_TEST(test_h_after_a_stopped_move_returns_the_partial_distance);
#endif
}
