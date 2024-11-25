# EnviLog

ESP32-S3 Environmental Monitoring System

## Project Overview
EnviLog is an embedded system project based on ESP32-S3 for environmental monitoring. It provides modular and scalable architecture for sensor integration, local display, and remote monitoring capabilities.

## Hardware
- Core Board: ESP32-S3-DevKitC-1-N32R8V
- Display: LCD1602 (Status Display)
- Sensors (Future Integration):
  - DHT11: Temperature/Humidity
  - Photoresistors: Light/Motion
  - HC-SR04: Distance

## Project Structure
```
envilog/
├── Makefile           # Build system configuration
├── core/              # Application logic
├── drivers/           # Device drivers
├── hal/               # Hardware Abstraction Layer
│   ├── esp32s3/      # MCU-specific definitions
│   ├── gpio/         # GPIO interface
│   ├── system/       # System functions
│   └── uart/         # UART interface
├── lib/              # Third-party libraries
└── startup/          # System initialization
```

## Development Environment
- Toolchain: xtensa-esp-elf-gcc (esp-13.2.0_20230928)
- Build System: Make
- Flash Tool: esptool.py
- Monitor: screen

## Build Instructions
```bash
# Build project
make

# Flash to device
make flash

# Monitor output
make monitor
```

## Project Status
- [x] Basic project structure
- [x] GPIO HAL implementation
- [x] Build system setup
- [ ] LCD driver
- [ ] Sensor integration
- [ ] Network connectivity
