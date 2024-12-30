# EnviLog

ESP32-S3 Environmental Monitoring System based on ESP-IDF framework

## Project Overview
EnviLog is an embedded system project based on ESP32-S3 for environmental monitoring. It provides modular and scalable architecture for sensor integration, local display, and remote monitoring capabilities.

## Hardware
- Core Board: ESP32-S3-DevKitC-1-N32R8V
- Display: LCD1602 (Status Display) - Future Integration
- Sensors (Future Integration):
  - DHT11: Temperature/Humidity
  - Photoresistors: Light/Motion
  - HC-SR04: Distance

## Project Structure
```
envilog/
├── CMakeLists.txt           # Project CMake configuration
├── README.md               
├── components/
│   ├── envilog_config/      # Project configurations
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── envilog_config.h
│   ├── envilog_mqtt/        # MQTT client implementation
│   │   ├── CMakeLists.txt
│   │   ├── envilog_mqtt.c
│   │   └── include/
│   │       └── envilog_mqtt.h
│   ├── http_server/         # HTTP server implementation
│   │   ├── CMakeLists.txt
│   │   ├── http_server.c
│   │   └── include/
│   │       └── http_server.h
│   ├── network_manager/     # WiFi and network handling
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── network_manager.h
│   │   └── network_manager.c
│   └── task_manager/        # FreeRTOS task management
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── system_monitor_msg.h
│       │   └── task_manager.h
│       ├── system_monitor_msg.c
│       └── task_manager.c
├── envilog_partitions.csv   # Custom partition table
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild    # Project configuration options
│   ├── include/
│   └── main.c              # Application entry point
├── sdkconfig                # Project configuration
├── sdkconfig.old           # Backup of previous configuration
└── www/                    # Frontend web files
    ├── css/
    │   └── styles.css
    ├── index.html
    └── js/
        └── main.js
```

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
idf.py menuconfig

# Build project
idf.py build

# Flash to device
idf.py -p PORT flash

# Monitor output
idf.py monitor
```

## Features
- System Monitoring
  * Real-time heap usage
  * System uptime
  * Task status
- Network Connectivity
  * WiFi station mode
  * Connection management
  * RSSI monitoring
- MQTT Integration
  * Broker connectivity
  * QoS-based message handling
  * Message queueing
  * Event monitoring
- Web Interface
  * Dashboard for system status
  * Real-time updates
  * RESTful API endpoints

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
- [x] Task Management
  - System monitor task
  - Network task
  - Event groups
  - Task priorities
  - Watchdog implementation
- [x] HTTP Server
  - REST API endpoints
  - Static file serving
  - Web dashboard
  - Error handling
  - Resource cleanup
- [x] Web Interface
   - Dashboard design and implementation
   - System status display
   - Network status display
   - Real-time updates
- [x] MQTT Integration
   - Broker connectivity
   - Topic structure
   - Message queuing
   - Event handling
   - Status monitoring
- [ ] System Integration
   - Configuration storage
   - OTA update system
   - System diagnostics expansion
   - Additional logging features
   - Enhanced error reporting
- [ ] Display system integration
- [ ] Sensor integration


## Version
Current version: v0.2.0
Features:
- Network stack implementation
- Task management framework
- System monitoring
- Error handling framework
- Web interface with dashboard
- Static file serving

## Acknowledgments
- ESP-IDF framework and documentation
- ESP32-S3 technical reference manual
