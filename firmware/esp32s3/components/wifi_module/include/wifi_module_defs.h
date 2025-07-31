#pragma once
#ifndef WIFI_MODULE_DEFS_H
#define WIFI_MODULE_DEFS_H

#include "stdint.h"

#include "esp_err.h"
#include "esp_wifi_types_generic.h"

#ifdef __cplusplus
extern "C" {
#endif

enum wifi_module_mode {
  WIFI_MODULE_MODE_STA,
  WIFI_MODULE_MODE_AP,
};

enum wifi_module_event {
  WIFI_MODULE_EVENT_ERROR = 0,
  WIFI_MODULE_EVENT_CONNECTED,
  WIFI_MODULE_EVENT_DISCONNECTED,
  WIFI_MODULE_EVENT_IP_ACQUIRED,
  WIFI_MODULE_EVENT_SCAN_DONE,

  WIFI_MODULE_EVENT_MAX,
};

enum wifi_module_state {
  WIFI_MODULE_STATE_ERROR = -1,
  WIFI_MODULE_STATE_INITIAL,
  WIFI_MODULE_STATE_STOPPED,
  WIFI_MODULE_STATE_RUNNING,

  WIFI_MODULE_STATE_MAX,
};

enum wifi_module_ap_state {
  WIFI_MODULE_AP_STATE_ERROR = -1,
  WIFI_MODULE_AP_STATE_INITIAL,
  WIFI_MODULE_AP_STATE_STOPPED,
  WIFI_MODULE_AP_STATE_RUNNING,

  WIFI_MODULE_AP_STATE_MAX,
};

enum wifi_module_sta_state {
  WIFI_MODULE_STA_STATE_ERROR = -1,
  WIFI_MODULE_STA_STATE_INITIAL,
  WIFI_MODULE_STA_STATE_DISCONNECTED,
  WIFI_MODULE_STA_STATE_CONNECTED,

  WIFI_MODULE_STA_STATE_MAX,
};

struct sta_config {
  const char *ssid;
  const char *password;
  wifi_auth_mode_t auth_mode;
  uint8_t max_retries;
  uint32_t retry_delay_ms;
};

struct ap_config {};
struct nan_config {};

struct wifi_module_config {
  union {
    struct sta_config sta;
    struct ap_config ap;
    struct nan_config nan;
  };
};

typedef struct wifi_module *wifi_module_handle;

#ifdef __cplusplus
}
#endif

#endif