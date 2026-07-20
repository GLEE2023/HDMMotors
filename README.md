# Three TMC5160 Motors + Dual Deployment Servos

PlatformIO project for an Arduino Nano controlling:

1. Lead-screw puck lift using millimeter-based target positions
2. Rotating barrel indexer using exact step counts
3. Precision yaw track from -30 to +30 degrees
4. Two deployment servos with reset, arm, and fire commands

The barrel-index and yaw movement logic from the precision-yaw project remains unchanged.

## PlatformIO

Open this folder in VS Code with PlatformIO and upload the `nanoatmega328` environment.

The Serial Monitor speed is:

```text
115200 baud
```

Libraries are installed through `platformio.ini`:

```ini
lib_deps =
    teemuatlut/TMCStepper
    arduino-libraries/Servo
```

## Pin map

### Stepper drivers

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

The original servo sketch used D3 and D4, but those pins are already used by the lead-screw driver. The integrated project uses:

| Servo | Nano pin |
|---|---:|
| Left servo signal | A3 |
| Right servo signal | A4 |

A3 and A4 are used as digital servo signal pins.

Power the servos from a suitable external regulated supply rather than from the Nano regulator. Connect the servo supply ground to the Nano/driver ground.

## Lead-screw easy configuration

The normal lead-screw settings are grouped at the top of `include/Config.h`:

```cpp
constexpr float PUCK_HEIGHT_MM = 14.0f;
constexpr float BARREL_HEIGHT_MM = 273.0f;
constexpr uint8_t PUCK_COUNT = 17;
constexpr float FIRST_PUCK_EXTRA_OFFSET_MM = 5.0f;
```

Deployment target positions are calculated as:

```text
level 0 = 0 mm
level 1 = offset + 1 puck = 5 + 14 = 19 mm
level 2 = offset + 2 pucks = 33 mm
...
level 17 = offset + 17 pucks = 243 mm
```

The `A` command is separate and moves to the full mechanical barrel height of 273 mm.

### Steps-per-millimeter calibration

Millimeters must still be converted to electrical STEP pulses. The project starts with:

```cpp
constexpr float LEAD_STEPS_PER_MM = 100.8f;
```

This was derived from the previous estimate of approximately 1512 pulses for 15 mm. It is a starting value, not a guaranteed mechanical calibration.

To recalibrate:

1. Command a known movement.
2. Measure the real movement in millimeters.
3. Calculate:

```text
new steps/mm = commanded steps / measured millimeters
```

After this one hardware calibration, puck height, barrel height, puck count, and first-puck offset are the routine settings.

## Servo behavior

The integrated servo movement is the same as the standalone sketch:

| State | Left servo | Right servo |
|---|---:|---:|
| Rest | 180° | 0° |
| Arm | 135° | 45° |
| Fire | 60° | 120° |

Both servos move in 2° increments with a 10 ms delay. Fire holds for 250 ms and then returns to rest.

## Serial commands

Commands are case-sensitive where noted because the original servo controls conflict with existing stepper commands.

### Deployment servos — lowercase

```text
w       Reset servos
e       Arm servos
p       Fire and return to rest
```

### Lead screw — uppercase

```text
W       Raise the next puck level
S       Lower to the previous puck level
A       Move to full mechanical top, 273 mm
D       Move to bottom and restore puck level 0
L       Treat the current physical position as bottom
```

The first `W` movement goes to 19 mm. Every later `W` advances one 14 mm puck level, up to level 17.

After `A`, the controller marks the puck level as unknown because 273 mm is not a normal puck level. Run `D` or physically home the mechanism and use `L` before using `W` or `S` again.

### Barrel indexer — unchanged

```text
N       Next index
P       Previous index
I5      Move to index 5
O       Set current barrel location as index 0
```

The barrel index count remains unchanged at 16 in this revision.

### Precision yaw — unchanged

```text
Y12.345 Absolute yaw angle
R0.025  Relative yaw movement
Q1      Move one configured yaw microstep
K       Release yaw holding torque
Z       Set current yaw position as zero
```

### General

```text
C       Print stepper and servo positions
X       Disable all stepper drivers
H, h, ? Print command help
```

Commands containing a number (`I`, `Y`, `R`, and `Q`) must be followed by a newline.

## Startup assumptions

The controller does not yet have homing sensors or encoders. At startup it assumes:

- Lead screw is at the bottom
- Barrel is at index 0
- Yaw is at 0 degrees
- Servos begin at their rest positions

If a stepper stalls, skips pulses, or is moved while disabled, its software position will no longer match its physical position.
