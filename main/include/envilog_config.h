#pragma once

// Project version
#define ENVILOG_VERSION_MAJOR    0
#define ENVILOG_VERSION_MINOR    1
#define ENVILOG_VERSION_PATCH    0

// System configuration
#define ENVILOG_TASK_WDT_TIMEOUT_MS    3000    // Task watchdog timeout
#define ENVILOG_DIAG_CHECK_INTERVAL_MS  5000    // Diagnostic check interval

// WiFi configuration
#define ENVILOG_WIFI_SSID       "your-ssid"      // WiFi SSID
#define ENVILOG_WIFI_PASS       "your-password"   // WiFi password
#define ENVILOG_WIFI_RETRY_NUM   5               // Maximum connection retries
#define ENVILOG_WIFI_CONN_TIMEOUT_MS  10000      // Connection timeout
