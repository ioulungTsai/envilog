# EnviLog

ESP32-S3 Environmental Monitoring System based on ESP-IDF framework

## Project Overview
EnviLog is an embedded system project based on ESP32-S3 for environmental monitoring. It provides modular and scalable architecture for sensor integration, local display, and remote monitoring capabilities.

## Hardware
- Core Board: ESP32-S3-DevKitC-1-N32R8V
- Display: LCD1602 (Status Display)
- Sensors (Future Integration):
  - DHT11: Temperature/Humidity
  - Photoresistors: Light/Motion
  - HC-SR04: Distance

## Project Structure (Temporary)
Current structure is based on ESP-IDF blink example. This will evolve as we add more features.
```
envilog/
├── components/        # Custom components
│   └── led_strip/    # LED control component
├── main/             # Main application code
│   ├── CMakeLists.txt
│   └── main.c       
├── CMakeLists.txt    # Project CMake configuration
└── sdkconfig         # Project configuration
```

Expected future additions:
- MQTT client component
- WiFi configuration
- Sensor drivers
- Web interface
- Data management

## Development Environment
- Framework: ESP-IDF v5.0
- Toolchain: ESP-IDF toolchain
- Build System: CMake
- IDE: VSCode
- Monitor: idf.py monitor

## Prerequisites
- ESP-IDF v5.0 or later
- CMake 3.16 or later
- Python 3.7 or later

## Build Instructions
```bash
# Configure project
idf.py set-target esp32s3

# Build project
idf.py build

# Flash to device
idf.py -p PORT flash

# Monitor output
idf.py -p PORT monitor

# Clean build files
idf.py clean
```

## Project Status
- [x] ESP-IDF framework integration
- [x] Basic project structure
- [x] Build system setup
- [x] LED blink verification
- [ ] LCD driver
- [ ] Sensor integration
- [ ] Network connectivity
