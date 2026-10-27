// Serial parsing rules from "Serial command interface > Parsing rules".
#include "test_helpers.h"

namespace {

bool parsesLong(const char *text, long expected) {
  long value = -12345;
  return parseLongValue(text, value) && value == expected;
}

bool rejectsLong(const char *text) {
  long value = 99;
  return !parseLongValue(text, value) && value == 99;
}

bool parsesFloat(const char *text, float expected) {
  float value = -12345.0f;
  return parseFloatValue(text, value) && value == expected;
}

bool rejectsFloat(const char *text) {
  float value = 99.0f;
  return !parseFloatValue(text, value) && value == 99.0f;
}

void test_long_plain_and_negative(void) {
  TEST_ASSERT_TRUE(parsesLong("5", 5));
  TEST_ASSERT_TRUE(parsesLong("0", 0));
  TEST_ASSERT_TRUE(parsesLong("-3", -3));
  TEST_ASSERT_TRUE(parsesLong("+4", 4));
}

void test_long_tolerates_separators_and_blanks(void) {
  TEST_ASSERT_TRUE(parsesLong(":5", 5));
  TEST_ASSERT_TRUE(parsesLong("=3", 3));
  TEST_ASSERT_TRUE(parsesLong(" 5", 5));
  TEST_ASSERT_TRUE(parsesLong(": 5", 5));
  TEST_ASSERT_TRUE(parsesLong("  : =  7", 7));
  TEST_ASSERT_TRUE(parsesLong("5   ", 5));
  TEST_ASSERT_TRUE(parsesLong("\t5\t", 5));
}

void test_long_rejects_junk_and_leaves_value_alone(void) {
  TEST_ASSERT_TRUE(rejectsLong(""));
  TEST_ASSERT_TRUE(rejectsLong("   "));
  TEST_ASSERT_TRUE(rejectsLong(":"));
  TEST_ASSERT_TRUE(rejectsLong("abc"));
  TEST_ASSERT_TRUE(rejectsLong("5x"));
  TEST_ASSERT_TRUE(rejectsLong("5 6"));
  TEST_ASSERT_TRUE(rejectsLong("1.5"));
  TEST_ASSERT_TRUE(rejectsLong("--5"));
}

void test_long_rejects_overflow(void) {
  TEST_ASSERT_TRUE(rejectsLong("99999999999999999999"));
  TEST_ASSERT_TRUE(rejectsLong("-99999999999999999999"));
}

void test_float_accepts_decimals_signs_and_separators(void) {
  TEST_ASSERT_TRUE(parsesFloat("12.5", 12.5f));
  TEST_ASSERT_TRUE(parsesFloat("-0.25", -0.25f));
  TEST_ASSERT_TRUE(parsesFloat("7", 7.0f));
  TEST_ASSERT_TRUE(parsesFloat(": 12.5", 12.5f));
  TEST_ASSERT_TRUE(parsesFloat("=-0.25", -0.25f));
  TEST_ASSERT_TRUE(parsesFloat(" 3.5  ", 3.5f));
}

void test_float_rejects_trailing_junk(void) {
  TEST_ASSERT_TRUE(rejectsFloat(""));
  TEST_ASSERT_TRUE(rejectsFloat("  "));
  TEST_ASSERT_TRUE(rejectsFloat("abc"));
  TEST_ASSERT_TRUE(rejectsFloat("1.5abc"));
  TEST_ASSERT_TRUE(rejectsFloat("1.5 2"));
  TEST_ASSERT_TRUE(rejectsFloat("1.5.2"));
}

void test_float_rejects_nan_infinity_and_overflow(void) {
  TEST_ASSERT_TRUE(rejectsFloat("nan"));
  TEST_ASSERT_TRUE(rejectsFloat("inf"));
  TEST_ASSERT_TRUE(rejectsFloat("-inf"));
  TEST_ASSERT_TRUE(rejectsFloat("1e999"));
  // Finite as a double but too large for a float.
  TEST_ASSERT_TRUE(rejectsFloat("1e39"));
}

// ---------------------------------------------------------------------------
// Line assembly (readSerialCommands). "Z" is a convenient harmless command.
// ---------------------------------------------------------------------------

const char kZeroYaw[] = "Current yaw position set as 0 degrees.";

void test_line_runs_only_at_newline(void) {
  Serial.feed("Z");
  readSerialCommands();
  TEST_ASSERT_EQUAL_STRING("", t::out().c_str());

  Serial.feed("\n");
  readSerialCommands();
  ASSERT_OUT_HAS(kZeroYaw);
}

void test_line_assembled_across_calls(void) {
  Serial.feed("Q");
  readSerialCommands();
  Serial.feed("\n");
  readSerialCommands();
  ASSERT_OUT_HAS("Unknown command.");
}

void test_carriage_return_is_ignored(void) {
  Serial.feed("Z\r\n");
  readSerialCommands();
  EQ(1, t::countOf(kZeroYaw));
}

void test_empty_and_blank_lines_do_nothing(void) {
  Serial.feed("\n\r\n   \n\t\n");
  readSerialCommands();
  TEST_ASSERT_EQUAL_STRING("", t::out().c_str());
}

void test_leading_spaces_and_tabs_are_ignored(void) {
  Serial.feed(" \t  Z\n");
  readSerialCommands();
  ASSERT_OUT_HAS(kZeroYaw);
}

void test_several_commands_in_one_burst(void) {
  Serial.feed("Z\nZ\nZ\n");
  readSerialCommands();
  EQ(3, t::countOf(kZeroYaw));
}

void test_31_character_command_runs(void) {
  Serial.feed(std::string("Z") + std::string(30, ' ') + "\n");
  readSerialCommands();
  ASSERT_OUT_HAS(kZeroYaw);
  ASSERT_OUT_LACKS("too long");
}

void test_32_character_command_is_discarded_not_truncated(void) {
  Serial.feed(std::string("Z") + std::string(31, ' ') + "\n");
  readSerialCommands();
  ASSERT_OUT_HAS("Command too long; discarded.");
  ASSERT_OUT_LACKS(kZeroYaw);
}

void test_overlong_line_never_runs_its_tail(void) {
  // The first 31 characters alone would be a valid I5 command.
  Serial.feed("I5" + std::string(40, ' ') + "\n");
  readSerialCommands();
  ASSERT_OUT_HAS("Command too long; discarded.");
  EQ(0, t::barrelPulses());
}

void test_line_after_overlong_line_still_runs(void) {
  Serial.feed(std::string(40, 'Q') + "\nZ\n");
  readSerialCommands();
  EQ(1, t::countOf("Command too long; discarded."));
  ASSERT_OUT_HAS(kZeroYaw);
}

void test_loop_polls_the_serial_port(void) {
  Serial.feed("?\n");
  loop();
  ASSERT_OUT_HAS("========== COMMANDS ==========");
}

}  // namespace

void run_parsing_tests() {
  RUN_TEST(test_long_plain_and_negative);
  RUN_TEST(test_long_tolerates_separators_and_blanks);
  RUN_TEST(test_long_rejects_junk_and_leaves_value_alone);
  RUN_TEST(test_long_rejects_overflow);
  RUN_TEST(test_float_accepts_decimals_signs_and_separators);
  RUN_TEST(test_float_rejects_trailing_junk);
  RUN_TEST(test_float_rejects_nan_infinity_and_overflow);
  RUN_TEST(test_line_runs_only_at_newline);
  RUN_TEST(test_line_assembled_across_calls);
  RUN_TEST(test_carriage_return_is_ignored);
  RUN_TEST(test_empty_and_blank_lines_do_nothing);
  RUN_TEST(test_leading_spaces_and_tabs_are_ignored);
  RUN_TEST(test_several_commands_in_one_burst);
  RUN_TEST(test_31_character_command_runs);
  RUN_TEST(test_32_character_command_is_discarded_not_truncated);
  RUN_TEST(test_overlong_line_never_runs_its_tail);
  RUN_TEST(test_line_after_overlong_line_still_runs);
  RUN_TEST(test_loop_polls_the_serial_port);
}
