# EnviLog

ESP32-S3 Environmental Monitoring System based on ESP-IDF framework

## Project Overview
EnviLog is an embedded system project based on ESP32-S3 for environmental monitoring. It provides modular and scalable architecture for sensor integration, local display, and remote monitoring capabilities.

## Hardware
- Core Board: ESP32-S3-DevKitC-1-N32R8V
- Sensors:
  - DHT11: Temperature/Humidity

## Project Structure
```
envilog/
├── CMakeLists.txt                   # Project CMake configuration
├── README.md               
├── components/
│   ├── builtin_led/                 # ESP32-S3 onboard LED control with status indication
│   │   ├── CMakeLists.txt
│   │   ├── builtin_led.c
│   │   └── include/
│   │       └── builtin_led.h
│   ├── data_manager/                # Centralized sensor data routing and management
│   │   ├── CMakeLists.txt
│   │   ├── data_manager.c
│   │   └── include/
│   │       └── data_manager.h
│   ├── dht11_sensor/                # DHT11 temperature/humidity sensor driver
│   │   ├── CMakeLists.txt
│   │   ├── dht11_sensor.c
│   │   └── include/
│   │       └── dht11_sensor.h
│   ├── envilog_config/              # Project-wide configuration definitions
│   │   ├── CMakeLists.txt
│   │   └── include/
│   │       └── envilog_config.h
│   ├── envilog_mqtt/                # MQTT client implementation
│   │   ├── CMakeLists.txt
│   │   ├── envilog_mqtt.c
│   │   └── include/
│   │       └── envilog_mqtt.h
│   ├── error_handler/               # Standardized error logging and categorization
│   │   ├── CMakeLists.txt
│   │   ├── error_handler.c
│   │   └── include/
│   │       └── error_handler.h
│   ├── http_server/                 # HTTP server implementation with REST API
│   │   ├── CMakeLists.txt
│   │   ├── http_server.c
│   │   └── include/
│   │       └── http_server.h
│   ├── network_manager/             # WiFi and network handling with dual-mode support
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── network_manager.h
│   │   └── network_manager.c
│   ├── system_manager/              # System configurations and diagnostics management
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   └── system_manager.h
│   │   └── system_manager.c
│   └── task_manager/                # FreeRTOS task management and monitoring
│       ├── CMakeLists.txt
│       ├── include/
│       │   ├── system_monitor_msg.h
│       │   └── task_manager.h
│       ├── system_monitor_msg.c
│       └── task_manager.c
├── dependencies.lock                # Component manager dependency lock file
├── envilog_partitions.csv          # Custom partition table
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild           # Project configuration options
│   ├── idf_component.yml           # Component manager manifest
│   ├── include/
│   └── main.c                      # Application entry point
├── sdkconfig                       # Project configuration
├── sdkconfig.old                   # Backup of previous configuration
└── www/                            # Frontend web files
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
- **System Monitoring**
  * Real-time heap usage
  * System uptime and CPU usage
  * Task status monitoring
  * Internal temperature monitoring
- **Network Connectivity**
  * Dual-mode operation (Station/AP modes)
  * Smart WiFi connection with automatic fallback
  * Echo-style boot logic for seamless setup
  * mDNS support (envilog.local access)
  * Connection resilience with retry mechanisms
- **Status Indication**
  * Onboard RGB LED with breathing effects
  * Visual network status (blue=Station, green=AP)
  * Gamma-corrected low-light visibility
- **MQTT Integration**
  * Broker connectivity with message queuing
  * QoS-based message handling
  * Sensor data publishing
  * Configuration via web interface
- **Web Interface**
  * Modern responsive dashboard
  * Real-time sensor data display
  * Network configuration with seamless switching
  * Password visibility controls
  * Toast notifications and modal guidance
  * RESTful API endpoints
- **Environmental Monitoring**
  * DHT11 temperature/humidity readings
  * Datasheet-based validation
  * Automatic error detection and recovery
  * Real-time data streaming

## Device Access
- **Station Mode**: http://[device-ip]/ or http://envilog.local/
- **AP Mode**: http://192.168.4.1/ (WiFi: "EnviLog", Password: "envilog1212")

## Project Status
- [x] Development environment setup
- [x] Project structure setup
- [x] Basic configuration
  - ESP-IDF SDK configured
  - Logging framework setup
  - Standardized error handling implemented
  - Basic diagnostics working
- [x] Network stack implementation
  - WiFi dual-mode (Station/AP)
  - Smart connection management
  - Automatic fallback mechanisms
  - mDNS support
- [x] Task Management
  - System monitor task
  - Network task
  - Event groups and queues
  - Task priorities
  - Watchdog implementation
- [x] HTTP Server
  - REST API endpoints
  - Static file serving
  - Web dashboard
  - SPIFFS integration
  - Resource cleanup
- [x] Web Interface
   - Modern responsive design
   - Real-time data updates
   - Network configuration UI
   - Enhanced user experience
- [x] MQTT Integration
   - Broker connectivity
   - Topic structure
   - Message queuing
   - Event handling
   - Configuration management
- [x] System Integration
   - Configuration storage (NVS)
   - System diagnostics
   - Error categorization
   - Component decoupling
- [x] Environmental Monitoring
   - DHT11 sensor integration
   - Temperature/humidity readings
   - Data validation and reliability
   - MQTT publishing
   - Web interface display
- [x] Status Indication
   - RGB LED with breathing effects
   - Network status visualization
   - Boot status indication

## Recent Improvements & Updates (Feb 2025 - July 2025)
1. **Code Quality Improvements**
   - Removed test functions from main.c for cleaner codebase
   - Implemented standardized error handling across all components
   - Enhanced DHT11 reliability with datasheet-based validation
   - Added data manager for decoupled sensor data flow

2. **User Experience Enhancements**
   - Added status LED with breathing effects and network indication
   - Implemented Echo-style WiFi setup with seamless transitions
   - Added mDNS support for envilog.local access
   - Complete frontend redesign with modal guidance and toast notifications
   - Enhanced password controls and form interactions

3. **System Architecture Improvements**
   - Dual-mode network system with automatic fallback
   - Smart retry mechanisms for connection resilience
   - Enhanced error logging with categorization
   - Improved component separation and maintainability

## Future Scope
1. Enhanced Display System
   - LCD1602 integration
   - Status display templates
   - Real-time sensor readings
2. Extended Sensor Suite
   - Light intensity monitoring
   - Distance/proximity sensing
   - Multi-sensor data fusion
3. Advanced System Features
   - Over-the-Air updates
   - Extended data storage
   - Advanced calibration
   - Backup systems
   - Analytics capabilities
4. System Optimizations
   - Power management
   - Deep sleep support
   - Extended diagnostics
   - Performance optimization
   - Security hardening

## Version
Current version: v0.3.0
Features:
- Complete environmental monitoring system
- Dual-mode networking with smart fallback
- Modern web interface with enhanced UX
- Status LED indication system
- Standardized error handling
- mDNS support for easy access

## Acknowledgments
- ESP-IDF framework and documentation
- ESP32-S3 technical reference manual
