#pragma once

// Project version
#define ENVILOG_VERSION_MAJOR    0
#define ENVILOG_VERSION_MINOR    1
#define ENVILOG_VERSION_PATCH    0

// System configuration
#define ENVILOG_TASK_WDT_TIMEOUT_MS    3000    // Task watchdog timeout
#define ENVILOG_DIAG_CHECK_INTERVAL_MS  5000    // Diagnostic check interval

// WiFi configuration
#define ENVILOG_WIFI_SSID       CONFIG_ENVILOG_WIFI_SSID
#define ENVILOG_WIFI_PASS       CONFIG_ENVILOG_WIFI_PASSWORD
#define ENVILOG_WIFI_RETRY_NUM  CONFIG_ENVILOG_WIFI_CONN_MAX_RETRY
#define ENVILOG_WIFI_CONN_TIMEOUT_MS CONFIG_ENVILOG_WIFI_CONN_TIMEOUT_MS

// MQTT configuration
#define ENVILOG_MQTT_BROKER_URL    CONFIG_ENVILOG_MQTT_BROKER_URL
#define ENVILOG_MQTT_CLIENT_ID     CONFIG_ENVILOG_MQTT_CLIENT_ID
#define ENVILOG_MQTT_KEEPALIVE     CONFIG_ENVILOG_MQTT_KEEPALIVE

// MQTT queue configuration
#define ENVILOG_MQTT_OUTBOX_SIZE       CONFIG_ENVILOG_MQTT_OUTBOX_SIZE
#define ENVILOG_MQTT_TIMEOUT_MS        CONFIG_ENVILOG_MQTT_TIMEOUT_MS
#define ENVILOG_MQTT_RETRY_TIMEOUT_MS  CONFIG_ENVILOG_MQTT_RETRY_TIMEOUT_MS
