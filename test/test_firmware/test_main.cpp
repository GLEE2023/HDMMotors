#include "test_helpers.h"

void setUp(void) { t::resetFirmware(); }

void tearDown(void) {}

int main(int, char **) {
  UNITY_BEGIN();

  run_parsing_tests();
  run_command_tests();
  run_lead_tests();
  run_barrel_yaw_tests();
  run_servo_tests();
  run_sensor_tests();
  run_stepper_tests();
  run_deployment_tests();
  run_startup_tests();

  return UNITY_END();
}
