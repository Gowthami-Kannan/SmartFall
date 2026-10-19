# SmartFall – Wearable Fall Detection and Emergency Alert System

## Overview

SmartFall is an ESP32-S3-based wearable fall detection and emergency alert prototype designed to identify potential falls using an MPU6050 accelerometer and gyroscope. It communicates with a paired Android phone through Bluetooth Low Energy (BLE) and supports cloud-based monitoring and event notifications through Blynk when Wi-Fi is available.

The system allows the user to confirm their safety or trigger an emergency alert using a push button.

## Features

- **Motion Monitoring:** Measures acceleration and angular velocity using the MPU6050.
- **Fall Detection:** Detects a preliminary low-acceleration event followed by a high-impact event.
- **Bluetooth Communication:** Sends event notifications to a connected Android application using BLE.
- **Manual SOS:** A single button press triggers a manual SOS event.
- **Safety Confirmation:** A double button press confirms that the user is safe.
- **Automatic Emergency Alert:** Triggers an SOS after a detected fall if the user does not respond within 12 seconds.
- **Blynk Cloud Integration:** Displays sensor readings, connectivity status, and emergency events.
- **Wi-Fi Reconnection:** Periodically attempts to reconnect when Wi-Fi becomes unavailable.
- **Independent BLE Operation:** BLE advertising is initialized independently of the Wi-Fi and Blynk connection.

## Hardware Requirements

- ESP32-S3-WROOM-1 N16R8
- MPU6050 accelerometer and gyroscope module
- Push button
- Android smartphone with a compatible BLE application
- USB cable and suitable power supply
- Wi-Fi connection for Blynk cloud features

## Pin Connections

| Component | ESP32-S3 Pin |
|---|---|
| MPU6050 SDA | GPIO 8 |
| MPU6050 SCL | GPIO 9 |
| Push button | GPIO 4 |
| MPU6050 VCC | According to module specifications |
| MPU6050 GND | GND |
| Push button other terminal | GND |

The push button uses the ESP32's internal pull-up resistor (`INPUT_PULLUP`). Therefore, pressing the button connects GPIO 4 to GND.

## Software Requirements

- Arduino IDE
- ESP32 board support package
- Adafruit MPU6050 library
- Adafruit Unified Sensor library
- ESP32 BLE libraries compatible with the installed ESP32 board package
- Blynk IoT account and device template
- Compatible Android BLE application

## Fall Detection Logic

The firmware samples the MPU6050 every 20 milliseconds and calculates the magnitude of acceleration and angular velocity.

### Detection parameters

| Parameter | Value |
|---|---:|
| Sensor sampling interval | 20 ms |
| Low-acceleration threshold | Below 0.55 g |
| Impact threshold | At least 2.5 g |
| Low-G detection window | 1200 ms |
| Button double-press window | 650 ms |
| Automatic SOS timeout | 12 seconds |
| Button debounce interval | 35 ms |

### Detection workflow

1. The MPU6050 continuously measures acceleration and angular velocity.
2. The ESP32-S3 calculates the total acceleration magnitude in g and the total angular velocity in degrees per second.
3. When acceleration falls below 0.55 g, the firmware records a possible low-acceleration event.
4. If acceleration subsequently reaches at least 2.5 g within 1200 milliseconds, the system declares a possible fall.
5. The firmware sends a `FALL_DETECTED` event through BLE and updates Blynk if connected.
6. The user has an opportunity to confirm safety with a double button press or indicate an emergency with a single press.
7. If no response is received, an automatic SOS is triggered after 12 seconds.

**Note:** This is a preliminary threshold-based detection algorithm. It does not yet use personalized adaptive learning, and its accuracy must be evaluated through controlled testing.

## Button Operation

| Button action | Behaviour |
|---|---|
| Single press | Triggers SOS after the 650 ms double-press window |
| Double press after a detected fall | Confirms that the user is safe |
| Double press after manual SOS | Confirms that the user is safe |
| No response after a detected fall | Triggers automatic SOS after 12 seconds |

A single press is interpreted as a manual SOS when no fall is pending. A double press when no fall or manual SOS is pending sends a `BUTTON_DOUBLE_PRESS` event.

## Bluetooth Low Energy (BLE)

The device advertises itself with the Bluetooth name `SmartFall`.

| BLE setting | Value |
|---|---|
| Device name | `SmartFall` |
| Service UUID | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Event characteristic UUID | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |
| Characteristic properties | Read and Notify |

The event characteristic communicates status and emergency messages to a connected BLE client.

### BLE event messages

- `SMARTFALL_READY`
- `FALL_DETECTED`
- `MANUAL_SOS`
- `SOS_EMERGENCY`
- `USER_SAFE`
- `BUTTON_DOUBLE_PRESS`

A compatible Android application must connect to the advertised service and subscribe to notifications to receive live events. This firmware does not itself implement an Android application.

## Blynk Cloud Integration

Blynk provides remote sensor monitoring, connectivity status, and event notifications when the device is connected to Wi-Fi and Blynk.

### Virtual pin configuration

| Virtual pin | Data |
|---|---|
| V0 | Total acceleration magnitude in g |
| V1 | Total angular velocity in degrees per second |
| V2 | Fall and monitoring status |
| V3 | SOS and emergency status |
| V4 | Wi-Fi connectivity status (1 = connected, 0 = disconnected) |

Sensor readings are uploaded every second.

### Blynk events

Configure the following event codes in your Blynk template:

- `fall_detected` — possible fall detected
- `sos_emergency` — manual or automatic emergency SOS

Configure the required Blynk dashboard widgets, event settings, and notifications in the Blynk Console.

**Important:** Blynk event notifications and cloud updates require an active Blynk connection. BLE notifications are sent only when a phone is connected. The current firmware does not store events locally for later cloud synchronization.

## Getting Started

### 1. Clone or download the repository

Download the repository from GitHub or clone it to your computer.

### 2. Install the required libraries

Open Arduino IDE and install the Adafruit MPU6050 and Adafruit Unified Sensor libraries through the Library Manager. Ensure that the BLE headers and Blynk library are available for your ESP32 board package.

### 3. Configure Blynk

Replace the placeholder values at the beginning of the source code with your own Blynk template ID, template name, and device authentication token.

### 4. Configure Wi-Fi

Replace the Wi-Fi placeholders with your local network credentials.

Do not publish real Wi-Fi credentials or Blynk authentication tokens in a public repository.

### 5. Connect the hardware

Connect the MPU6050 to GPIO 8 and GPIO 9 for I2C communication. Connect the push button between GPIO 4 and GND.

### 6. Upload the firmware

1. Connect the ESP32-S3 to your computer.
2. Select the correct ESP32-S3 board and COM port.
3. Compile and upload the sketch.
4. Open Serial Monitor at 115200 baud.
5. Check that the MPU6050 is detected and BLE starts successfully.

### 7. Test the system

Connect a compatible Android BLE application to `SmartFall`. If Wi-Fi and Blynk are configured, verify the sensor dashboard and event notifications.

Test the button logic and alert pathway using safe, controlled simulations. Do not deliberately perform dangerous falls.

## Project Structure

```text
SmartFall-Detection/
├── SmartFall.ino
├── README.md
├── LICENSE
├── .gitignore
└── docs/
    ├── circuit-diagram.png
    └── block-diagram.png
```

The `docs` folder and diagram files are optional. Add them when the diagrams are available.

## Limitations

- Threshold-based fall detection can produce false positives and false negatives.
- The algorithm has not been clinically validated.
- BLE event delivery requires an active phone connection.
- Blynk cloud monitoring requires Wi-Fi and a working Blynk connection.
- Events generated while Blynk is offline are not queued for automatic later upload.
- The Android application must be implemented or configured separately.
- This firmware is not a certified medical or emergency response device.

## Future Enhancements

- Personalized adaptive fall detection.
- Improved false-alarm reduction using motion patterns.
- A dedicated Android application with emergency contacts.
- Offline event logging and recovery after network reconnection.
- Battery monitoring and low-power operation.
- Evaluation using recorded motion data and controlled testing.

## Author

**Gowthami K**  

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
