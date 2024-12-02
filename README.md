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

## Project Structure
```
envilog/
├── main/
│   ├── envilog_config.h    # Project configurations
│   ├── main.c             # Main application code
│   └── CMakeLists.txt     # Main component CMake
├── CMakeLists.txt         # Project CMake configuration
└── sdkconfig              # Project configuration
```

Future additions:
- Network components
- Display drivers
- Sensor drivers
- Web interface
- MQTT client
- Data management

## Development Environment
- Framework: ESP-IDF v5.0
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
- [x] Development environment setup
- [x] Project structure setup
- [x] Basic configuration complete
  - ESP-IDF SDK configured
  - Logging framework setup
  - Error handling implemented
  - Basic diagnostics working
- [ ] Network stack implementation
- [ ] Display system integration
- [ ] Sensor integration

## Version
Current version: v0.1.0
- Basic system configuration
- Diagnostic monitoring
- Error handling framework

## Contributing
1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request

## License
This project is licensed under the MIT License - see the LICENSE file for details

## Acknowledgments
- ESP-IDF framework and documentation
- ESP32-S3 technical reference manual
