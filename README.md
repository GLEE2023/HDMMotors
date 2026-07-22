# GLEE HDM Puck Delivery Controller

This project is a PlatformIO firmware for an Arduino Nano that controls a small puck-delivery mechanism. It combines three stepper motors, two firing servos, and a simple serial command interface so the system can be operated manually or through a higher-level controller.

The firmware is organized around four main behaviors:

1. A lead-screw elevator that raises and lowers pucks to different levels.
2. A rotating barrel indexer that selects the next chamber to fire from.
3. A yaw axis that can aim the firing direction over a limited range.
4. Two deployment servos that reset, arm, and fire the puck.

## What the Project is For

This firmware is meant to drive a mechanical puck launcher with a single-button fire workflow, but it also supports manual control over serial. In practice, the system can:

- detect whether a puck is present at the firing position
- fire one puck at a time
- advance the lead screw for the next puck level
- rotate the barrel to the next chamber
- return the elevator to the bottom reference position
- report the current state of the motors and servos

## Project Layout

The repository is split into a small set of source and header files:

- [src/main.cpp](src/main.cpp): top-level serial command parser and startup/loop logic
- [src/Axes.cpp](src/Axes.cpp): control logic for lead screw, barrel, and yaw movement
- [src/StepperController.cpp](src/StepperController.cpp): low-level TMC5160 driver setup and step pulse generation
- [src/ServoController.cpp](src/ServoController.cpp): servo reset/arm/fire behavior
- [include/Config.h](include/Config.h): hardware pins, motion profiles, servo angles, and mechanical constants

## Quick Start

1. Install PlatformIO and open this folder in VS Code.
2. Connect the Arduino Nano to USB.
3. Build and upload the `nanoatmega328` environment from [platformio.ini](platformio.ini).
4. Open the serial monitor at 115200 baud.
5. Try a simple command such as `e` to arm the servos, `C` to print the current system status, or `T` to run the TMC5160 SPI connection test.

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

lib_deps =
    teemuatlut/TMCStepper
    arduino-libraries/Servo
```

For serial debugging, the default monitor settings are set to 115200 baud with newline handling enabled.

## Hardware Overview and Pinout

This project uses three TMC5160 stepper drivers and two hobby servos. The Nano pin mapping is defined in [include/Config.h](include/Config.h).

### Stepper Drivers

| Function | Nano pin |
|---|---:|
| Lead STEP | D2 |
| Lead DIR | D3 |
| Lead ENABLE | D4 |
| Barrel STEP | D5 |
| Barrel DIR | D6 |
| Barrel ENABLE | D7 |
| Yaw STEP | A0 |
| Yaw DIR | A1 |
| Yaw ENABLE | A2 |
| Yaw SPI CS | D8 |
| Barrel SPI CS | D9 |
| Lead SPI CS | D10 |
| Shared MOSI | D11 |
| Shared MISO | D12 |
| Shared SCK | D13 |

### Servos

| Servo | Nano pin |
|---|---:|
| Left servo signal | A3 |
| Right servo signal | A4 |

The servos should be powered from a suitable external 5 V supply, not from the Nano regulator. The servo ground and the Nano/driver ground should be connected together.

## Important Configuration Values

Most of the machine behavior is controlled from [include/Config.h](include/Config.h). The values you are most likely to adjust are:

- `PUCK_HEIGHT_MM`: the vertical spacing between puck levels
- `BARREL_HEIGHT_MM`: the overall travel height of the lead screw
- `PUCK_COUNT`: how many puck levels the system understands
- `FIRST_PUCK_EXTRA_OFFSET_MM`: the extra offset applied before the first puck level
- `LEAD_STEPS_PER_MM`: the step-per-millimeter calibration for the lead screw
- `BARREL_POSITION_COUNT`: how many barrel indexes exist
- `YAW_MINIMUM_DEGREES` and `YAW_MAXIMUM_DEGREES`: the allowed yaw range
- servo angle constants such as `LEFT_SERVO_REST`, `LEFT_SERVO_ARM`, and `LEFT_SERVO_FIRE`

The lead-screw calibration is especially important. The current value is a starting point and should be adjusted if the real motion does not match the commanded motion.

## Motion and Behavior Notes

A few behaviors are worth knowing before using the machine:

- The elevator is tracked in terms of puck levels and also in physical step position.
- The barrel is treated as a circular indexer, so movement is optimized to take the shortest route to the requested chamber.
- The yaw axis is limited to a mechanical range and is clamped to that range when commands are issued.
- The servo motion is smooth by default, and the fire action briefly holds the fire position before returning to rest.
- The firmware assumes the system is already roughly homed at startup: the lead screw starts at the bottom, the barrel at index 0, and yaw at 0 degrees.

## Serial Command Reference

The firmware listens for commands over the serial port at 115200 baud. Commands are processed when a newline is received.

### General Commands

| Command | Purpose |
|---|---|
| `F` or `f` | Run one full puck deployment cycle if a puck is present |
| `H`, `h`, or `?` | Print the available commands |
| `C` | Print the current status of the axes and servos |
| `T` or `t` | Run the TMC5160 SPI connection test over the serial monitor |
| `X` | Disable all stepper drivers |

### Servo Commands

| Command | Purpose |
|---|---|
| `w` | Reset servos to the rest position |
| `e` | Move servos to the armed position |
| `p` | Fire once and return to rest |

### Lead-Screw Commands

| Command | Purpose |
|---|---|
| `W` | Raise the elevator by one puck level |
| `S` | Lower the elevator by one puck level |
| `A` | Move the lead screw to the full top position |
| `D` | Move the lead screw back to the bottom reference |
| `L` | Treat the current physical position as the new bottom reference |

### Barrel Commands

| Command | Purpose |
|---|---|
| `N` | Move the barrel to the next chamber index |
| `P` | Move the barrel to the previous chamber index |
| `I5` | Move the barrel directly to chamber index 5 |
| `O` | Set the current barrel position as index 0 |

### Yaw Commands

| Command | Purpose |
|---|---|
| `Y12.5` | Move to an absolute yaw angle in degrees |
| `R0.25` | Move by a relative yaw angle in degrees |
| `Z` | Set the current yaw position as zero |

## Notes for New Users

The firmware does not currently use encoders or automatic homing sensors. If the mechanism is moved by hand, stalls, skips steps, or is changed while a motor is disabled, the software position can become inaccurate. In that case, the physical position should be re-established with the appropriate `L`, `O`, or `Z` command.
