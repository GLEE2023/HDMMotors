# GLEE HDM Puck Delivery Controller

This project is a PlatformIO firmware for an Arduino Nano that controls a small puck-delivery mechanism. It combines a lead-screw stepper, a barrel indexer, a yaw axis, two firing servos, and a simple serial command interface so the system can be operated manually or through a higher-level controller.

The firmware is organized around four main behaviors:

1. A lead-screw elevator that raises and lowers pucks to different levels.
2. A rotating barrel indexer that selects the next chamber to fire from.
3. A yaw axis that can aim the firing direction over a limited range.
4. Two deployment servos that reset, arm, and fire the puck.

## What the Project is For

This firmware drives the mechanism through a serial command interface. In practice, the system can:

- detect puck presence when the switch is configured, or assume a puck is present without one
- fire one puck at a time
- run the complete staged deployment sequence for all configured pucks
- advance the lead screw for the next puck level
- rotate the barrel to the next chamber when the barrel solenoid is configured
- return the elevator to the bottom reference position
- report the tracked axis positions, configured switch states, and servo angles

## Project Layout

The repository is split into a small set of source and header files:

- [src/main.cpp](src/main.cpp): top-level serial command parser and startup/loop logic
- [src/Axes.cpp](src/Axes.cpp): control logic for lead screw, barrel, and yaw movement
- [src/StepperController.cpp](src/StepperController.cpp): low-level motion driver setup for the lead-screw TMC5160 and standalone barrel/yaw STEP/DIR control
- [src/ServoController.cpp](src/ServoController.cpp): servo reset/arm/fire behavior
- [src/Sensors.cpp](src/Sensors.cpp): limit/puck switches, burnwire output, and barrel solenoid (all optional pins)
- [include/Config.h](include/Config.h): hardware pins, motion profiles, servo angles, and mechanical constants
- [test/test_firmware/](test/test_firmware): host-side unit tests with fake Arduino hardware (see below)

## Running Unit Tests

The tests compile the real firmware sources on your PC (no board needed) against fake Arduino, Servo, SPI and TMCStepper headers that record every pin write, delay and servo move. They check the behavior listed in `.github/agents/PROTOTYPE_CONSIDERATIONS.md`.

Requires a host C/C++ compiler on `PATH` (gcc/g++, or clang via MinGW/MSYS2):

```
pio test -e native             # optional pins left UNUSED_PIN, as on the bench
pio test -e native_configured  # test-only pins for switches, solenoid, burnwire and enables
```

The two environments cover both wiring states. The `HDM_OPTIONAL_PIN(benchPin, testPin)` macro in [Config.h](include/Config.h) picks `benchPin` on hardware; to wire a real pin, edit its first argument. The AVR build is unaffected (`pio run`).

## Quick Start

1. Install PlatformIO and open this folder in VS Code.
2. Connect the Arduino Nano to USB.
3. Build and upload the `nanoatmega328` environment from [platformio.ini](platformio.ini).
4. Open the serial monitor at 115200 baud.
5. Try a simple command such as `e` to arm the servos, `C` to print the current system status, or `T` to run the lead-screw TMC5160 SPI connection test.

The firmware targets an Arduino Nano-compatible board and uses the following PlatformIO settings:

### Architecture Overview

```text
+----------------------+
|   Arduino Nano      |
|----------------------|
| Serial Commands      |
|  - parse/dispatch    |
|  - status reporting  |
+----------+-----------+
           |
           +---------------------------+
           |                           |
+----------v----------+    +--------v---------+
| Stepper Controller  |    | Servo Controller |
| - TMC5160 drivers   |    | - reset/arm/fire |
| - step generation   |    +------------------+
+----------+----------+
           |
+----------v----------+
| Axes Controller     |
| - lead screw        |
| - barrel indexer    |
| - yaw axis          |
+---------------------+
```

```ini
[env:nanoatmega328]
platform = atmelavr
board = nanoatmega328new
framework = arduino

monitor_speed = 115200
monitor_echo = yes
monitor_eol = LF
monitor_filters = send_on_enter

lib_deps =
    teemuatlut/TMCStepper
    arduino-libraries/Servo
```

For serial debugging, the default monitor settings are set to 115200 baud with newline handling enabled.

## Hardware Overview and Pinout

This project uses one TMC5160 stepper driver for the lead screw and two standalone TMC2209 STEP/DIR drivers for the barrel and yaw axes, plus two servos. The Nano pin mapping is defined in [include/Config.h](include/Config.h).

### Stepper Drivers

| Function | Nano pin |
|---|---:|
| Lead STEP | D6 |
| Lead DIR | D7 |
| Barrel STEP | D4 |
| Barrel DIR | D5 |
| Yaw STEP | D2 |
| Yaw DIR | D3 |
| Lead SPI CS | D10 |
| Shared MOSI | D11 |
| Shared MISO | D12 |
| Shared SCK | D13 |

### Servos

| Servo | Nano pin |
|---|---:|
| Left servo signal | A1 |
| Right servo signal | A2 |

The servos should be powered from a suitable external 5 V supply, not from the Nano regulator. The servo ground and the Nano/driver ground should be connected together.

The driver enable pins, burnwire trigger, bottom limit switch, puck-detection switch, and barrel solenoid are currently set to `UNUSED_PIN` (`255`) in the firmware. `H` returns to the configured bottom, using the bottom switch when present and otherwise the software-tracked position. `B` ignores that reference and drives down for up to the full configured travel, stopping early if the bottom switch trips. Without driver enable pins, stopping step pulses does not remove holding torque. Barrel indexing proceeds without the solenoid but warns that the barrel was not unlocked. The full deployment warns that puck presence cannot be verified without its switch. Configure relevant pins before relying on switch-based homing, puck detection, driver disabling, barrel release, or burnwire output.

## Important Configuration Values

The active values from [include/Config.h](include/Config.h) are listed below.

### Lead Screw

| Setting | Current value |
|---|---:|
| Puck-level spacing | 14 mm |
| Configured travel height | 256 mm |
| Puck levels | 17 |
| First-level offset from bottom | 13 mm |
| Calibration | 100.8 steps/mm |
| Driver current | 1400 mA |
| Microsteps | 4 |
| Motion profile (start / cruise / acceleration) | 2400 / 1400 us / 300 steps |

### Barrel and Yaw

| Setting | Barrel | Yaw |
|---|---:|---:|
| Microsteps | 8 | 8 |
| Motor full steps/revolution | 200 | 200 |
| Gear ratio | 111:20 (5.55:1) | 111:20 (5.55:1) |
| Indexes / angle limits | 6 indexes | -30 to +30 degrees |
| Motion profile (start / cruise / acceleration) | 1800 / 700 us / 100 steps | 2400 / 1200 us / 160 steps |

Yaw is configured to hold its position after a move. The barrel and yaw drivers are standalone STEP/DIR drivers, so their current is set in hardware, not by this firmware. The lead-screw calibration is especially important and should be adjusted if physical travel does not match commanded travel.

### Servos and Timing

| Setting | Left servo | Right servo |
|---|---:|---:|
| Rest angle | 180 degrees | 0 degrees |
| Armed angle | 135 degrees | 45 degrees |
| Fire angle | 40 degrees | 140 degrees |

Servo movements change by 5 degrees every 20 ms; the fire position is held for 300 ms. Step pulses are high for 5 us, with a 5 ms direction-settle delay before each move. The configured sense resistor is 0.075 ohm and serial baud is 115200.

## Motion and Behavior Notes

A few behaviors are worth knowing before using the machine:

- The elevator is tracked in terms of puck levels and also in physical step position.
- The barrel is treated as a circular indexer, so movement is optimized to take the shortest route to the requested chamber.
- The yaw axis is limited to a mechanical range; out-of-range `Y`/`R` requests are clamped to that range and the clamp is reported.
- Typing `x` while a stepper move is running stops that move and the tracked position follows the steps actually taken. The lead puck level becomes unknown until `H` or `B` re-establishes it, except that stopping `B` with `x` sets the stop point as the new bottom (position 0).
- The barrel and yaw motors are driven as standalone TMC2209 STEP/DIR motors without UART, so their motion is controlled by step timing rather than driver configuration registers.
- The servo motion is smooth by default, and the fire action briefly holds the fire position before returning to rest.
- The firmware assumes the system is already roughly homed at startup: the lead screw starts at the bottom, the barrel at index 0, and yaw at 0 degrees.

The full deployment command starts from the bottom and raises to puck level 1, then for each puck raises one puck height (`PUCK_HEIGHT_MM`) into the deployment position, arms both servos, lowers 3 mm, waits for Enter, fires, then raises 3 mm to realign. The next puck's raise starts from that realigned position, repeating until `PUCK_COUNT` pucks have been deployed (the last puck is fired at 251 mm of the 256 mm travel). After the final cycle, the elevator returns to the bottom using the limit switch when configured, or the software-tracked reference otherwise.

## Serial Command Reference

The firmware listens for commands over the serial port at 115200 baud. Commands are processed when a newline is received.

### General Commands

| Command | Purpose |
|---|---|
| `G` or `g` | Run the staged deployment sequence for every configured puck, then return the elevator to the bottom reference |
| `H` or `h` | Return to the configured bottom (switch if configured, otherwise software reference) |
| `B` | Drive down for the full configured travel, stopping early if the bottom switch trips. Typing `x` stops it and sets that point as position 0 |
| `?` | Print the available commands |
| `C` | Print the current status of the axes and servos |
| `T` or `t` | Run the TMC5160 SPI connection test over the serial monitor |
| `X` | Stop step pulses and disable drivers with configured enable pins (between moves; during a move, type `x` to stop it) |
| `U` or `u` | Pulse the burnwire output for 12 seconds |

### Servo Commands

| Command | Purpose |
|---|---|
| `w` | Reset servos to the rest position |
| `e` | Move servos to the armed position |
| `m` or `M` | Fire both servos and return them to rest |

### Lead-Screw Commands

| Command | Purpose |
|---|---|
| `W` | Raise the elevator by one puck level (the first level includes the extra first-puck offset) |
| `S` | Lower the elevator by one puck level |
| `+` | Move the lead screw up by 1 mm for alignment checks |
| `-` | Move the lead screw down by 1 mm for alignment checks |

### Barrel Commands

| Command | Purpose |
|---|---|
| `N` | Move the barrel to the next chamber index |
| `P` | Move the barrel to the previous chamber index |
| `I5` | Move the barrel directly to chamber index 5 |

### Yaw Commands

| Command | Purpose |
|---|---|
| `Y12.5` | Move to an absolute yaw angle in degrees |
| `R0.25` | Move by a relative yaw angle in degrees |
| `Z` | Set the current yaw position as zero |

## Notes for New Users

The firmware does not currently use encoders. The bottom and puck-detection switches are unassigned in the current configuration, so physical homing and switch-based puck detection are not available. If the mechanism is moved by hand, stalls, skips steps, or is changed while a motor is disabled, the software position can become inaccurate. Re-establish the physical position before relying on automatic motion; `Z` only resets the yaw reference.
