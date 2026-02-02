#ifndef WIFI
#define WIFI

#include <esp_wifi.h>             // For Wi-Fi functions and configurations
#include <esp_log.h>              // For logging

#include "wifi_config.h"

#define WIFI_TAG "WIFI"
typedef void (*update_screenf_callback_t)(const char* fmt, ...);

typedef enum {
    WIFI_SUCCESS = 1 << 0,
    WIFI_FAILURE = 1 << 1,
    TCP_SUCCESS = 1 << 0,
    TCP_FAILURE = 1 << 1,
    MAX_FAILURES = 10
} wifi_status_t;

esp_err_t connect_wifi(void);

#endif /* WIFI */
