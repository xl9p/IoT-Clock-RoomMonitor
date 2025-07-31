#pragma once
#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include "wifi_module_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t
wifi_module_del();

esp_err_t
wifi_module_init();

esp_err_t
wifi_module_start();
esp_err_t
wifi_module_stop();

esp_err_t
wifi_module_enable_mode(enum wifi_module_mode mode, const struct wifi_module_config *module_cfg);
esp_err_t
wifi_module_disable_mode(enum wifi_module_mode mode);

// STA related API
esp_err_t
wifi_module_sta_scan();

// AP related API

// NAN related API

// Configuration
esp_err_t
wifi_module_set_config();

// Partial configuration/reconfiguration

// Event handlers
esp_err_t
wifi_module_register_event_handler(enum wifi_module_event event_type, esp_event_handler_t event_handler);
esp_err_t
wifi_module_unregister_event_handler(enum wifi_module_event event_type);

#ifdef __cplusplus
}
#endif

#endif