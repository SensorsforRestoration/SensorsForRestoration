
# Pin Table — Seeed Studio XIAO ESP32‑S3 Sense

The table below assigns pins for:

- Four DC motor pumps driven by two L9110 dual motor drivers
- One salinity sensor (two excitation pins + one analog read)
- One depth sensor using a UART interface (RX/TX)

All GPIOs on the XIAO ESP32‑S3 are 3.3 V logic. Ensure every module connected to the MCU’s IO uses 3.3 V logic levels. Tie all grounds together.

## Assignments

### Option A — Independent Direction (default)

| Subsystem | Signal | XIAO label | GPIO | Direction | Notes |
|---|---|---|---:|---|---|
| Motor Driver A (L9110 #1) | M1 IN1 | `D0 / A0` | 1 | MCU→Driver | PWM-capable (LEDC) |
| Motor Driver A (L9110 #1) | M1 IN2 | `D1 / A1` | 2 | MCU→Driver | Direction/Brake |
| Motor Driver A (L9110 #1) | M2 IN1 | `D2 / A2` | 3 | MCU→Driver | PWM-capable |
| Motor Driver A (L9110 #1) | M2 IN2 | `D3 / A3` | 4 | MCU→Driver | Direction/Brake |
| Motor Driver B (L9110 #2) | M3 IN1 | `D4 / SDA / A4` | 5 | MCU→Driver | PWM-capable; shares I2C SDA if used |
| Motor Driver B (L9110 #2) | M3 IN2 | `D5 / SCL / A5` | 6 | MCU→Driver | Shares I2C SCL if used |
| Motor Driver B (L9110 #2) | M4 IN1 | `D8 / SCK / A8` | 7 | MCU→Driver | PWM-capable |
| Motor Driver B (L9110 #2) | M4 IN2 | `D9 / MISO / A9` | 8 | MCU→Driver | Direction/Brake |
| Salinity Sensor | Excitation A | `D11 / A11` | 12 | MCU→Sensor | Bottom pad; see board diagram |
| Salinity Sensor | Excitation B | `D12 / A12` | 13 | MCU→Sensor | Bottom pad; see board diagram |
| Salinity Sensor | Analog Read | `D10 / A10` | 9 | Sensor→MCU | ADC input (0–3.3 V) |
| Depth Sensor (UART) | MCU TX → Sensor RX | `TX / D6` | 43 | MCU→Sensor | 3.3 V UART TX |
| Depth Sensor (UART) | MCU RX ← Sensor TX | `RX / D7` | 44 | Sensor→MCU | 3.3 V UART RX |
| Power | 3V3 | `3V3` | — | — | Use for 3.3 V devices (logic reference) |
| Power | 5V | `5V` | — | — | Use only if the peripheral is 5 V tolerant; motors typically powered separately |
| Ground | GND | `GND` | — | — | Common ground for MCU, drivers, and sensors |

### Option B — Shared Direction (sequential pump operation)

This variant ties all L9110 “direction” inputs together so every pump reverses together, while you only enable one pump at a time. This reduces GPIO count but has electrical caveats; read the notes below carefully.

| Subsystem | Signal | XIAO label | GPIO | Direction | Notes |
|---|---|---|---:|---|---|
| All Motors | DIR_ALL → each channel’s second input | `D5 / SCL / A5` | 6 | MCU→Driver | Shared line; LOW=forward, HIGH=reverse (see caveats) |
| Motor 1 | EN/PWM (to M1 first input) | `D0 / A0` | 1 | MCU→Driver | PWM for speed; only raise for active pump |
| Motor 2 | EN/PWM (to M2 first input) | `D2 / A2` | 3 | MCU→Driver | PWM for speed |
| Motor 3 | EN/PWM (to M3 first input) | `D8 / SCK / A8` | 7 | MCU→Driver | PWM for speed |
| Motor 4 | EN/PWM (to M4 first input) | `D9 / MISO / A9` | 8 | MCU→Driver | PWM for speed |
| Salinity Sensor | Excitation A | `D11 / A11` | 12 | MCU→Sensor | Bottom pad |
| Salinity Sensor | Excitation B | `D12 / A12` | 13 | MCU→Sensor | Bottom pad |
| Salinity Sensor | Analog Read | `D10 / A10` | 9 | Sensor→MCU | ADC input (0–3.3 V) |
| Depth Sensor (UART) | MCU TX → Sensor RX | `TX / D6` | 43 | MCU→Sensor | 3.3 V UART TX |
| Depth Sensor (UART) | MCU RX ← Sensor TX | `RX / D7` | 44 | Sensor→MCU | 3.3 V UART RX |
| Power | 3V3 | `3V3` | — | — | Logic reference |
| Ground | GND | `GND` | — | — | Common ground |

## Wiring Notes

- L9110 drivers: Each DC motor uses two inputs; one pin can be PWM for speed. Typical drive patterns: Forward = IN1=PWM, IN2=LOW; Reverse = IN1=LOW, IN2=PWM; Brake = IN1=HIGH, IN2=HIGH. Power the motors from a separate supply sized for the pumps; connect grounds to the XIAO GND.
- I2C pins (`D4/SDA`, `D5/SCL`): This plan uses these as motor control. If you later add I2C devices, remap M3 pins to free GPIOs instead (e.g., `D10/A10` and a bottom pad) and return `D4/D5` to I2C.
- Salinity sensor: Drive an alternating excitation on `D11`/`D12` to reduce electrode polarization, then sample on `A10`. Add a resistor network per your probe design to keep the ADC within 0–3.3 V.
- Depth sensor (UART): Connect sensor TX→`RX/D7` and sensor RX←`TX/D6`. Provide the sensor with 3.3 V TTL UART levels. Many depth modules require 5 V power but still use 3.3 V logic—check your module’s datasheet.

### Notes for Option B (Shared Direction)

- Electrical behavior: The L9110 does not provide a dedicated enable per channel. If you physically tie all “direction” inputs together (`DIR_ALL`) and feed each motor a single per-channel PWM line, reverse speed control becomes limited because L9110 expects the PWM to be applied on the input corresponding to the selected direction. In the simplest wiring, reverse acts as on/off. If you require PWM in reverse, add a small gating stage (e.g., a 74HC4053/74HC4066 or two NPN/NMOS gates per channel) to route the per-motor PWM to the appropriate L9110 input based on `DIR_ALL`.
- One-at-a-time rule: With `DIR_ALL`, ensure only one motor’s PWM is non‑zero at any time, otherwise more than one pump will run together.
- Alternative drivers: TB6612FNG or DRV8833 provide clearer PHASE/ENABLE control and per‑channel standby/enable, making shared‑direction + per‑motor enable straightforward in hardware.

## Power Guidance

- Motors: Use an external supply appropriate for the pumps. Route this to the `VCC`/`VM` of both L9110 boards; do not draw motor power from the XIAO. Place flyback suppression if your pump is inductive (most DC pumps are).
- MCU and sensors: Use the XIAO `3V3` for logic and low-power sensors; verify current budget. Always share ground across all modules.

## Quick Reference (by pin)

| XIAO Pin | Function here |
|---|---|
| `D0` | M1 IN1 |
| `D1` | M1 IN2 |
| `D2` | M2 IN1 |
| `D3` | M2 IN2 |
| `D4` | M3 IN1 |
| `D5` | M3 IN2 |
| `D6 (TX)` | Depth sensor RX (connect to sensor RX) |
| `D7 (RX)` | Depth sensor TX (connect to sensor TX) |
| `D8` | M4 IN1 |
| `D9` | M4 IN2 |
| `D10 (A10)` | Salinity analog read |
| `D11 (A11)` | Salinity excitation A |
| `D12 (A12)` | Salinity excitation B |
| `3V3` | 3.3 V rail for logic/sensors |
| `5V` | 5 V rail (USB or external) |
