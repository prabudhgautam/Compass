# STEM LAB: Multi-Sensor I2C Dashboard (Arduino Nano)

A three-device I2C system on an Arduino Nano (ATmega328P): an SSD1306 OLED, an ADXL345 accelerometer and a QMC5883L magnetometer sharing one bus. It reads both sensors, derives a magnetic heading, and renders the result on the display and over serial.

This was not a plug-and-play build. The first integrated firmware failed at display initialisation, and the fault turned out to be electrical (SDA held low), not software. The debugging record is documented in full below because it is the main engineering content of this project.

---

## System overview

```text
                 +-------------------------------+
                 |  Arduino Nano (ATmega328P)    |
                 |  2 KB SRAM, 32 KB flash       |
                 |                               |
                 |   A4 (SDA) ----+-----+-----+  |
                 |   A5 (SCL) ----+-----+-----+  |
                 +-----------------|-----|-----|--+
                                   |     |     |
                           +-------+  +--+--+  +-------+
                           |SSD1306|  |ADXL |  |QMC5883|
                           | 0x3C  |  | 345 |  |  L    |
                           | OLED  |  | 0x53|  | 0x0D  |
                           +-------+  +-----+  +-------+

   Serial (USB) -> raw values + init trace for debugging
```

Data path:

```text
ADXL345 --(I2C)--> getEvent() --> accel x,y,z --+
                                                 +--> format --> OLED + Serial
QMC5883L --(I2C)--> read()    --> mag x,y,z ----+
                                  |
                                  +--> atan2(y, x) --> 0..360 deg --> 8-way direction
```

## Hardware

| Part | Role | Interface | I2C address |
|---|---|---|---|
| Arduino Nano | MCU | n/a | n/a |
| SSD1306 0.96" OLED | Display | I2C | `0x3C` |
| ADXL345 | 3-axis accelerometer | I2C (CS tied high) | `0x53` |
| GY-271 module (QMC5883L) | 3-axis magnetometer | I2C | `0x0D` |

Bus pins on the Nano: **A4 = SDA, A5 = SCL**. All three devices sit on the same two lines and are distinguished only by address.

### Wiring

| Signal | OLED | ADXL345 | GY-271 |
|---|---|---|---|
| Power | VDD (check module rating) | VCC to 3.3V | VCC (check module rating) |
| Ground | GND | GND | GND |
| SDA | A4 | A4 | A4 |
| SCL | A5 | A5 | A5 |
| Other | n/a | CS to 3.3V, SDO to GND, INT1/INT2 NC | DRDY NC |

ADXL345 pin notes: tying **CS high** selects I2C mode (low would select SPI). Tying **SDO low** sets the address to `0x53` (high would give `0x1D`). Both are easy to get wrong, and a wrong CS or SDO connection is a plausible contributor to the bus problem described below.

## Software

| Function | Library |
|---|---|
| Display | Adafruit GFX, Adafruit SSD1306 |
| Accelerometer | Adafruit ADXL345 Unified |
| Magnetometer | MechaQMC5883 |

**Part identification matters.** The module is silkscreened GY-271, which normally means an HMC5883L (address `0x1E`). An I2C scan returned `0x0D` instead, which identifies a QMC5883L. The two chips have different registers and need different libraries, so the scan prevented a wrong-driver dead end.

## Heading computation

Current implementation:

```text
heading = atan2(magY, magX) * 180/pi
if heading < 0: heading += 360
```

`atan2` returns -180..+180 degrees, so negative results are wrapped into 0..360. The heading is then binned into 8 sectors of 45 degrees centred on each compass point (N covers 337.5 to 22.5, NE covers 22.5 to 67.5, and so on).

This is a **raw, uncorrected heading**. It assumes the sensor is level, and it includes the hard-iron offset of the board and any nearby magnetised or ferrous material. It does not apply magnetic declination. Treat the displayed direction as a demonstration of the pipeline, not as a navigation-grade reading.

### Sanity checks on live data

A few quick checks from the logged samples:

| Sensor | Sample | Magnitude | Expected |
|---|---|---|---|
| ADXL345 | (0.43, -0.43, 9.06) | about 9.08 m/s^2 | 9.81 (gravity) when static |
| ADXL345 | (0.27, -0.98, 9.14) | about 9.20 m/s^2 | 9.81 |

The accelerometer reads roughly 6 to 7 percent low on total magnitude while stationary, which points to uncorrected gain or offset error. Axis calibration is on the roadmap. For the magnetometer, the useful check will be that field magnitude stays constant as the board is rotated; deviation from that is how hard-iron and soft-iron error will be quantified.

---

## Integration failure and root cause analysis

### Symptom

The first firmware combining all three devices halted at display init:

```text
Starting OLED...
OLED FAILED
```

### Method

The approach was to form hypotheses, then test each one in isolation so that exactly one variable changed per step. The firmware printed a trace line before every init step, which made it clear where execution stopped.

### Hypothesis elimination

| # | Hypothesis | Test | Result | Verdict |
|---|---|---|---|---|
| 1 | OLED hardware, wiring or driver is faulty | OLED alone, all other devices removed | Initialised and displayed correctly | Ruled out |
| 2 | OLED and ADXL345 conflict | Both together | Both initialised; ADXL read about (0.27, -0.98, 9.14) | Ruled out |
| 3 | QMC5883L is dead or misconfigured | QMC5883L alone | Live changing values, about (-333, 2222, -3147) | Ruled out |
| 4 | Wrong magnetometer chip or driver | I2C scan | `0x0D` found, so QMC5883L, not HMC5883L | Corrected driver choice |
| 5 | Including the QMC library breaks OLED init | Library included with OLED, never called | OLED still worked | Ruled out |
| 6 | SRAM exhaustion | Compiler report plus runtime free-SRAM probe | 569 bytes of globals (27 percent), 1479 bytes free at startup and before OLED init | Ruled out |
| 7 | Bus-level fault | Wire timeout test per address | Transactions hung; timeout flagged even against `0x3C` | Bus fault confirmed |
| 8 | A line is stuck | Read A4 and A5 as raw pins | SDA = 0, SCL = 1 | **SDA held low** |
| 9 | Which device holds it | Disconnect ADXL345 SDA | SDA = 1, SCL = 1 | Fault tied to the ADXL345 connection |
| 10 | Wiring is wrong | Re-wire ADXL345 against the datasheet pin roles | Both lines idle high | Bus recovered |
| 11 | Full bus healthy | Final scan with all three connected | `0x0D`, `0x3C`, `0x53` found, 3 devices | Verified |

### Why SDA low breaks everything

I2C uses open-drain lines with pull-ups, so an idle bus reads high and any device can pull a line low. If one device holds SDA low, no START condition can be generated and every transaction to every address fails or hangs, including transactions to devices that are themselves healthy. That is why the OLED, which worked alone, reported failure once the faulty connection was present. The error appeared at the OLED only because it was the first device initialised, not because it was the cause.

### Root cause

The ADXL345 connection was pulling SDA low. Disconnecting its SDA restored an idle-high bus, and a careful re-wire restored normal operation. I did not isolate the exact physical fault beyond that (a miswired pin, a bad contact, or a CS or SDO strap issue are all consistent with the evidence), so I am not claiming a more specific cause than I verified.

### Verification run

Final scan:

```text
FOUND: 0x0D
FOUND: 0x3C
FOUND: 0x53
Total devices: 3
```

Final integrated bring-up:

```text
I2C READY
Starting OLED...        OLED SUCCESS
Starting ADXL345...     ADXL345 SUCCESS
Starting QMC5883L...    QMC SUCCESS
ADXL X: 0.43  Y: -0.43  Z: 9.06
QMC  X: -1140 Y: 38     Z: -253
OLED UPDATED
SETUP COMPLETE
```

---

## Resource notes

| Resource | Figure |
|---|---|
| SRAM total | 2048 bytes |
| Global data | 569 bytes (27 percent) |
| Free at startup | 1479 bytes |

One thing worth watching: if the display is a 128x64 SSD1306, the Adafruit driver allocates a 1024-byte frame buffer at `begin()`. That would leave roughly 450 bytes for stack, other libraries and future code (calibration tables, filters). The startup figure of 1479 bytes was measured, but the post-init figure above is my arithmetic, not a measurement, so SRAM headroom should be re-measured after display init before adding anything large.

## Limitations

- Heading is uncalibrated and assumes the sensor is level.
- No hard-iron or soft-iron correction.
- No magnetic declination applied.
- ADXL345 magnitude reads low (see sanity checks), so no offset or gain calibration yet.
- No filtering: raw samples are shown directly.
- No bus recovery logic: a stuck-low line is only detected manually, not handled in firmware.

## Roadmap

1. **Magnetometer calibration.** Hard-iron offsets from per-axis min/max while rotating through all orientations, then soft-iron scale correction. Validate with a constant-magnitude check.
2. **Accelerometer calibration.** Per-axis offset and gain against known orientations.
3. **Tilt compensation.** Derive roll and pitch from the accelerometer and project the magnetic vector onto the horizontal plane before `atan2`.
4. **Sensor fusion and filtering.** Smooth the heading and reduce jitter.
5. **Bus robustness.** Add a Wire timeout and a startup check of SDA/SCL idle state, with the standard clock-pulse bus recovery sequence if SDA is found low, and a clear on-screen error instead of a silent hang.
6. **Memory budget.** Re-measure SRAM after display init and move constant strings to flash if needed.

## Engineering takeaways

- **Verify the part, not the label.** Silkscreen said HMC5883L-style; the bus said QMC5883L.
- **Measure before assuming.** Both the memory theory and the library theory were plausible and both were wrong. Each was settled with a measurement or an isolated test, not a guess.
- **Debug bottom-up.** Power, wiring, bus electrical state, address detection, driver init, readings, processing, display. A failure high in the stack can originate low in it, as here.
- **Make the failure location visible.** Per-step serial trace lines turned "it doesn't work" into "it stops at OLED init".
- **Integrate incrementally.** One new device at a time kept every failure attributable to one change.
- **Check readings against physics.** Gravity magnitude is a cheap, free reference that exposes calibration error.

## Project structure

```text
Compass/
├── Compass.ino
└── README.md
```

## Author

Prabudh Gautam. Robotics software and embedded sensing.

GitHub: [@prabudhgautam](https://github.com/prabudhgautam)
