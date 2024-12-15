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
├── components/
│   ├── envilog_config/        # Project configurations
│   │   └── include/
│   │       └── envilog_config.h
│   ├── network_manager/       # WiFi and network handling
│   │   └── include/
│   │       └── network_manager.h
│   └── task_manager/         # FreeRTOS task management
│       └── include/
│           └── task_manager.h
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild     # Project configuration options
│   └── main.c               # Application entry point
└── CMakeLists.txt           # Project CMake configuration
```

Future additions:
- Display driver component
- Sensor driver components
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
- [x] Network stack implementation
  - WiFi station mode
  - Connection management
  - Error recovery
- [x] Task Management (Partial)
  - System monitor task
  - Network task
  - Event groups
  - Task priorities
- [ ] Display system integration
- [ ] Sensor integration
- [ ] Web interface
- [ ] MQTT integration

## Version
Current version: v0.2.0
- Network stack implementation
- Task management framework
- System monitoring
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
