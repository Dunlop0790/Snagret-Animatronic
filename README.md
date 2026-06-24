# Burrowing Snagret Animatronic

A wireless animatronic neck controller inspired by the Burrowing Snagret from Pikmin. Built as a school project with the goal of going beyond basic servo control and learning BLE communication, state machines, and sensor integration.

<img width="1920" height="2550" alt="c42695fb-c0c6-43a1-8c07-7d5171e41c56" src="https://github.com/user-attachments/assets/bbde52b7-41ac-4d31-a92e-d261ae12c6cf" />
<img width="908" height="495" alt="image" src="https://github.com/user-attachments/assets/e46238ef-671b-4711-85aa-a0eff00bfbee" />
<img width="908" height="456" alt="image" src="https://github.com/user-attachments/assets/c28719d1-c55c-45f9-936b-3c02d54a6d94" />

---

## Demo

https://github.com/user-attachments/assets/your-video-id-here

## Overview

The system consists of two ESP32 microcontrollers communicating over Bluetooth Low Energy. A handheld controller reads two analog joysticks and streams positional data to the robot at 33Hz. The robot receives that data and drives four servos through a PCA9685 PWM driver, with easing applied to make motion feel smooth and organic.

The neck mechanism is a cable-driven continuum design with servo-actuated tendons controlling two independent segments: lower and upper.

---

## Hardware

### Controller
- ESP32 development board
- 2x analog thumbstick joysticks
- 1S LiPo battery
- TP4056 charge module
- 3.7V to 5V boost converter

### Robot
- ESP32 development board
- PCA9685 16-channel PWM servo driver (I2C)
- 4x 20kg servos (2 per neck segment)
- 3S LiPo battery
- 5V buck converter

---

## Wiring

### Controller Joystick Pins
| Joystick | Axis | ESP32 Pin |
|----------|------|-----------|
| Left | X | GPIO 34 |
| Left | Y | GPIO 35 |
| Right | X | GPIO 32 |
| Right | Y | GPIO 33 |

> All joystick pins use ADC1. ADC2 pins conflict with BLE on the ESP32 and will give unreliable readings when the radio is active.

### Robot I2C (PCA9685)
| Signal | ESP32 Pin |
|--------|-----------|
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Robot Servo Channels (PCA9685)
| Channel | Function |
|---------|----------|
| 0 | Lower segment X |
| 1 | Lower segment Y |
| 2 | Upper segment X |
| 3 | Upper segment Y |

---

## How It Works

### Connection
The controller scans for any BLE device advertising a matching service UUID and connects automatically on startup. No manual pairing or hardcoded addresses are needed. A watchdog runs every loop cycle and restores the connection silently if it drops.

### Data Transmission
Joystick values are read as 12-bit ADC samples (0 to 4095) and packed into an 8-byte binary packet, two bytes per axis. Packets are sent write-without-response at 30ms intervals for low latency continuous streaming.

### Servo Control
The robot receives packets via a BLE callback that fires instantly on arrival. Joystick values are normalized to a -1.0 to +1.0 range with a 5% deadzone applied around center to eliminate jitter at rest. An exponential easing function is applied each frame so servos decelerate naturally as they approach their target angle rather than snapping there instantly.

### Thread Safety
The BLE callback and main loop run on separate cores of the ESP32. Joystick globals are declared `volatile` to prevent stale caching, and a per-frame local snapshot is taken before any servo math runs to avoid mid-calculation data corruption from the BLE thread.

---

## Configuration

These values can be tuned in `SnagretRobot.ino` to match your specific hardware:

```cpp
const float EASE_ALPHA = 0.5f;   // easing speed: 0.0 (slow) to 1.0 (instant)
float RANGE_LOWER_X   = 35.0f;   // degrees of travel per segment axis
float RANGE_LOWER_Y   = 35.0f;
float RANGE_UPPER_X   = 35.0f;
float RANGE_UPPER_Y   = 35.0f;
const int SERVO_MIN   = 100;     // PCA9685 pulse count at 0 degrees
const int SERVO_MAX   = 500;     // PCA9685 pulse count at 180 degrees
```

---

## Libraries Required

- [ESP32 BLE Arduino](https://github.com/espressif/arduino-esp32)
- [Adafruit PWM Servo Driver](https://github.com/adafruit/Adafruit-PWM-Servo-Driver-Library)

---

## Project Status

The core wireless control system is functional. Planned features still in development:

- Button input on joystick click pins for preset animation triggers
- State machine (manual / idle sway / strike / burrow)
- Alternate articulated ball-and-socket spine replacing the current cable backbone
- Keyframe and parametric animation system
- Head sculpt, silicone skin, fur, and eyes

---

