# Deivi Oximeter

Arduino sketch for a pulse oximeter built with an ESP32 and the MAX30102 sensor.

## What it does

- Measures heart rate (BPM) and blood oxygen saturation (SpO2) over a 15-second window
- Filters out noise and sudden spikes for more stable readings
- Ignores the first 3 seconds to let the signal stabilize
- Prints results to the Serial Monitor at 115200 baud

## Hardware

| Component | Detail |
|-----------|--------|
| Microcontroller | ESP32 |
| Sensor | MAX30102 (connected via I2C) |
| SDA pin | GPIO 21 |
| SCL pin | GPIO 22 |

## Dependencies

Install these libraries through the Arduino Library Manager:

- `SparkFun MAX3010x Pulse and Proximity Sensor Library`

## Usage

1. Wire the MAX30102 to the ESP32 (SDA → GPIO 21, SCL → GPIO 22, 3.3 V, GND)
2. Upload the sketch via Arduino IDE
3. Open the Serial Monitor at **115200 baud**
4. Place your finger firmly on the sensor
5. Wait 15 seconds — results print automatically

## Output example

```
Place your finger on the sensor for 15 seconds.
Measurement started...
Elapsed: 1 seconds
Elapsed: 2 seconds
...
=== RESULTS ===
Heart Rate: 72.4 bpm | SpO2: 98.2%
===============
```
