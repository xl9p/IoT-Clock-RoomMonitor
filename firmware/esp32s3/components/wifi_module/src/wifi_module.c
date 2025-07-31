#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "private/wifi_module_private.h"
#include "wifi_module.h"
#include "wifi_module_defs.h"

struct wifi_ap_ctx {
  bool enabled;

  enum wifi_module_ap_state state;

  esp_netif_t *netif;
};

struct wifi_sta_ctx {
  bool enabled;

  enum wifi_module_sta_state state;
  uint32_t current_retries;
  uint32_t max_retries;

  esp_netif_t *netif;
};

struct wifi_module {
  enum wifi_module_state state;
  struct wifi_ap_ctx ap_ctx;
  struct wifi_sta_ctx sta_ctx;

  // If more events are desired, consider using an array of structs
  // Also, separate into different mode contexts
  EventGroupHandle_t event_group;
  struct event_handler_t {
    esp_event_handler_t error_event;
  } event_handlers;
};

static const char *TAG = "wifi_module";

static struct wifi_module wifi_module_global = {
    .state = WIFI_MODULE_STATE_INITIAL,

    .ap_ctx.enabled = false,
    .ap_ctx.state = WIFI_MODULE_AP_STATE_INITIAL,

    .sta_ctx.enabled = false,
    .sta_ctx.state = WIFI_MODULE_STA_STATE_INITIAL,
    .sta_ctx.current_retries = 0,
    .sta_ctx.max_retries = 0,
};

// typedef esp_err_t (*wifi_event_handler_t)(wifi_module_handle wifi_module, enum wifi_module_event event_type, void *event_data);
// void (*esp_event_handler_t)(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

// WIFI_MODULE_EVENT_ERROR
// WIFI_MODULE_EVENT_CONNECTED
// WIFI_MODULE_EVENT_DISCONNECTED
// WIFI_MODULE_EVENT_IP_ACQUIRED
// WIFI_MODULE_EVENT_SCAN_DONE

static void
on_error_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void
on_sta_connected_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void
on_sta_disconnected_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void
on_sta_ip_acquired_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void
on_sta_scan_done_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void
on_sta_start_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void
update_module_state(wifi_module_handle module, enum wifi_module_state new_state);
static void
update_ap_state(wifi_module_handle module, enum wifi_module_ap_state new_state);
static void
update_sta_state(wifi_module_handle module, enum wifi_module_sta_state new_state);

esp_err_t
wifi_init_softap(const struct ap_config *ap_cfg, esp_netif_t **out_netif);
esp_err_t
wifi_init_sta(const struct sta_config *sta_cfg, esp_netif_t **out_netif);

void
softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta);

esp_err_t
wifi_module_del() {
  ESP_RETURN_ON_FALSE(wifi_module_global.state != WIFI_MODULE_STATE_INITIAL, ESP_ERR_INVALID_STATE, TAG, "invalid module state");

  if (wifi_module_global.event_group) {
    vEventGroupDelete(wifi_module_global.event_group);
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_deinit());

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &on_sta_start_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_sta_connected_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_sta_disconnected_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &on_sta_scan_done_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_sta_ip_acquired_event_handler));
  update_module_state(&wifi_module_global, WIFI_MODULE_STATE_INITIAL);
  return ESP_OK;
}

esp_err_t
wifi_module_init() {
  esp_err_t ret = ESP_OK;

  ESP_GOTO_ON_FALSE(wifi_module_global.state == WIFI_MODULE_STATE_INITIAL, ESP_ERR_INVALID_STATE, err, TAG,
                    "invalid module state");

  wifi_module_global.event_group = xEventGroupCreate();
  ESP_GOTO_ON_FALSE(wifi_module_global.event_group, ESP_ERR_NO_MEM, err, TAG, "not enough mem to allocate event_group member");

  ESP_GOTO_ON_ERROR(esp_netif_init(), err, TAG, "failed to initialize netif component");

  ESP_GOTO_ON_ERROR(
      esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, &on_sta_start_event_handler, NULL, NULL), err, TAG,
      "failed to register an event handler for wifi events");
  ESP_GOTO_ON_ERROR(
      esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_sta_connected_event_handler, NULL, NULL), err,
      TAG, "failed to register an event handler for wifi events");
  ESP_GOTO_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                        &on_sta_disconnected_event_handler, NULL, NULL),
                    err, TAG, "failed to register an event handler for wifi events");
  ESP_GOTO_ON_ERROR(
      esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &on_sta_scan_done_event_handler, NULL, NULL), err,
      TAG, "failed to register an event handler for wifi events");
  ESP_GOTO_ON_ERROR(
      esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_sta_ip_acquired_event_handler, NULL, NULL), err, TAG,
      "failed to register an event handler for ip events");

  // WIFI_EVENT_AP_START,                 /**< Soft-AP start */
  // WIFI_EVENT_AP_STOP,                  /**< Soft-AP stop */
  // WIFI_EVENT_AP_STACONNECTED,          /**< A station connected to Soft-AP */
  // WIFI_EVENT_AP_STADISCONNECTED,       /**< A station disconnected from Soft-AP */
  // WIFI_EVENT_AP_PROBEREQRECVED,        /**< Receive probe request packet in soft-AP interface */

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_GOTO_ON_ERROR(esp_wifi_init(&cfg), err, TAG, "failed to init wifi");

  wifi_module_global.event_handlers.error_event = on_error_event_handler;

  update_module_state(&wifi_module_global, WIFI_MODULE_STATE_STOPPED);

  return ESP_OK;
err:
  if (wifi_module_global.event_group) {
    vEventGroupDelete(wifi_module_global.event_group);
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_deinit());

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_START, &on_sta_start_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &on_sta_connected_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(
      esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &on_sta_disconnected_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &on_sta_scan_done_event_handler));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_sta_ip_acquired_event_handler));
  update_module_state(&wifi_module_global, WIFI_MODULE_STATE_INITIAL);
  return ret;
}

esp_err_t
wifi_module_start() {
  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(wifi_module_global.state == WIFI_MODULE_STATE_STOPPED &&
                          (wifi_module_global.ap_ctx.enabled || wifi_module_global.sta_ctx.enabled),
                      ESP_ERR_INVALID_STATE, TAG, "invalid state");

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());

  EventBits_t bits = xEventGroupWaitBits(wifi_module_global.event_group, WIFI_STA_CONNECTED_BIT | WIFI_STA_ERROR_BIT, pdFALSE,
                                         pdFALSE, portMAX_DELAY);

  if (bits & WIFI_STA_CONNECTED_BIT) {
    ESP_LOGD(TAG, "Connected to access point");
    update_sta_state(&wifi_module_global, WIFI_MODULE_STA_STATE_CONNECTED);
  } else if (bits & WIFI_STA_ERROR_BIT) {
    ESP_LOGW(TAG, "Failed to connect to the access point");
    update_sta_state(&wifi_module_global, WIFI_MODULE_STA_STATE_DISCONNECTED);
    ret = ESP_FAIL;
  } else {
    ESP_LOGE(TAG, "UNEXPECTED EVENT");
    ret = ESP_FAIL;
  }
  update_module_state(&wifi_module_global, WIFI_MODULE_STATE_RUNNING);
  return ret;
}
esp_err_t
wifi_module_stop() {
  esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
  return ret;
}

esp_err_t
wifi_module_enable_mode(enum wifi_module_mode mode, const struct wifi_module_config *module_cfg) {
  ESP_RETURN_ON_FALSE(module_cfg, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

  switch (mode) {
  case WIFI_MODULE_MODE_STA: {
    ESP_RETURN_ON_FALSE(!wifi_module_global.sta_ctx.enabled, ESP_ERR_INVALID_STATE, TAG, "STA mode is already enabled");

    ESP_RETURN_ON_ERROR(wifi_init_sta(&module_cfg->sta, &wifi_module_global.sta_ctx.netif), TAG,
                        "failed to initialize sta wifi interface");
    wifi_module_global.sta_ctx.enabled = true;

    // ESP_RETURN_ON_ERROR(esp_wifi_set_max_tx_power(19U), TAG, "Failed to set max tx power");

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode((wifi_module_global.ap_ctx.enabled) ? WIFI_MODE_APSTA : WIFI_MODE_STA), TAG,
                        "failed to set wifi sta mode");
    update_sta_state(&wifi_module_global, WIFI_MODULE_STA_STATE_DISCONNECTED);
    break;
  };
  case WIFI_MODULE_MODE_AP: {
    ESP_RETURN_ON_FALSE(!wifi_module_global.ap_ctx.enabled, ESP_ERR_INVALID_STATE, TAG, "AP mode is already enabled");

    ESP_RETURN_ON_ERROR(wifi_init_softap(&module_cfg->ap, &wifi_module_global.ap_ctx.netif), TAG,
                        "failed to initialize ap wifi interface");
    wifi_module_global.ap_ctx.enabled = true;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode((wifi_module_global.sta_ctx.enabled) ? WIFI_MODE_APSTA : WIFI_MODE_AP), TAG,
                        "failed to set wifi ap mode");
    update_sta_state(&wifi_module_global, WIFI_MODULE_AP_STATE_STOPPED);
    break;
  };
  default: {
    ESP_LOGE(TAG, "unsupported wifi mode");
    return ESP_ERR_NOT_SUPPORTED;
  };
  };
  return ESP_OK;
}
esp_err_t
wifi_module_disable_mode(enum wifi_module_mode mode) {
  esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
  return ret;
}

// Private API functions
static void
update_module_state(wifi_module_handle module, enum wifi_module_state new_state) {
  ESP_LOGD(TAG, "switching wifi module state, from:%lu to:%lu", (uint32_t)module->state, (uint32_t)new_state);
  module->state = new_state;
}
static void
update_ap_state(wifi_module_handle module, enum wifi_module_ap_state new_state) {
  ESP_LOGD(TAG, "switching wifi ap state, from:%lu to:%lu", (uint32_t)module->ap_ctx.state, (uint32_t)new_state);
  module->ap_ctx.state = new_state;
}
static void
update_sta_state(wifi_module_handle module, enum wifi_module_sta_state new_state) {
  ESP_LOGD(TAG, "switching wifi sta state, from:%lu to:%lu", (uint32_t)module->sta_ctx.state, (uint32_t)new_state);
  module->sta_ctx.state = new_state;
}

esp_err_t
wifi_init_softap(const struct ap_config *ap_cfg, esp_netif_t **out_netif) {
  esp_err_t ret = ESP_ERR_NOT_SUPPORTED;
  // esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();
  // wifi_config_t wifi_ap_config = {
  //     .ap =
  //         {
  //             .ssid = ap_cfg->ssid,
  //             .channel = EXAMPLE_ESP_WIFI_CHANNEL,
  //             .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
  //             .max_connection = EXAMPLE_MAX_STA_CONN,
  //             .authmode = WIFI_AUTH_WPA2_PSK,
  //             .pmf_cfg =
  //                 {
  //                     .required = false,
  //                 },
  //         },
  // };

  // if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) {
  //   wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
  // }

  // ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

  // ESP_LOGI(TAG_AP, "wifi_init_softap finished. SSID:%s password:%s channel:%d", EXAMPLE_ESP_WIFI_AP_SSID,
  //          EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);
  return ret;
}
esp_err_t
wifi_init_sta(const struct sta_config *sta_cfg, esp_netif_t **out_netif) {
  esp_err_t ret = ESP_OK;

  esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

  wifi_config_t wifi_sta_config = {
      .sta =
          {
              .scan_method = WIFI_ALL_CHANNEL_SCAN,
              .failure_retry_cnt = sta_cfg->max_retries,
              .threshold.authmode = sta_cfg->auth_mode,
              .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
          },
  };

  strncpy((char *)wifi_sta_config.sta.ssid, sta_cfg->ssid, sizeof(wifi_sta_config.sta.ssid));
  strncpy((char *)wifi_sta_config.sta.password, sta_cfg->password, sizeof(wifi_sta_config.sta.password));

  wifi_sta_config.sta.ssid[sizeof(wifi_sta_config.sta.ssid) - 1] = '\0';
  wifi_sta_config.sta.password[sizeof(wifi_sta_config.sta.password) - 1] = '\0';

  ESP_GOTO_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config), err, TAG, "failed to set sta wifi config");

  ESP_LOGD(TAG, "wifi sta initialized");

  *out_netif = esp_netif_sta;
  return ESP_OK;
err:
  if (esp_netif_sta) {
    esp_netif_destroy_default_wifi(esp_netif_sta);
  }
  return ret;
}

void
softap_set_dns_addr(esp_netif_t *esp_netif_ap, esp_netif_t *esp_netif_sta) {
  esp_netif_dns_info_t dns;
  esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns);
  uint8_t dhcps_offer_option = DHCPS_OFFER_DNS;
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_stop(esp_netif_ap));
  ESP_ERROR_CHECK(esp_netif_dhcps_option(esp_netif_ap, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER, &dhcps_offer_option,
                                         sizeof(dhcps_offer_option)));
  ESP_ERROR_CHECK(esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns));
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_dhcps_start(esp_netif_ap));
}

static void
on_error_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGE(TAG, "error event");
}
static void
on_sta_connected_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGD(TAG, "connected event");
}
static void
on_sta_disconnected_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  uint32_t curr_retries = wifi_module_global.sta_ctx.current_retries;
  uint32_t max_retries = wifi_module_global.sta_ctx.max_retries;

  if (curr_retries < max_retries) {
    esp_wifi_connect();
    ESP_LOGD(TAG, "retry to connect to the AP, tries %lu/%lu", curr_retries, max_retries);
    wifi_module_global.sta_ctx.current_retries++;
  } else {
    xEventGroupSetBits(wifi_module_global.event_group, WIFI_STA_ERROR_BIT);
  }
  ESP_LOGE(TAG, "connect to the AP failed");
}
static void
on_sta_ip_acquired_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
  if (NULL == event)
    return;

  ESP_LOGD(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
  wifi_module_global.sta_ctx.current_retries = 0;
  xEventGroupSetBits(wifi_module_global.event_group, WIFI_STA_CONNECTED_BIT);
}
static void
on_sta_scan_done_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_LOGD(TAG, "scan done event");
}
static void
on_sta_start_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());
}

// void from_sta_to_ap(void){
//     printf("## Entering from_sta_to_ap.\n");
//     esp_netif_ip_info_t ipInfo;
//     if(ap_sta == 1){
//         ESP_ERROR_CHECK(esp_wifi_disconnect());
//         ESP_ERROR_CHECK(esp_wifi_stop());
//         ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
//     }
//     IP4_ADDR(&ipInfo.ip, 192,168,2,1);
// 	IP4_ADDR(&ipInfo.gw, 192,168,2,1);
// 	IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
// 	esp_netif_dhcps_stop(wifiAP);
// 	esp_netif_set_ip_info(wifiAP, &ipInfo);
// 	esp_netif_dhcps_start(wifiAP);
//     ESP_LOGI(TAG, "IP: " IPSTR, IP2STR(&ipInfo.ip));
//     ESP_LOGI(TAG, "GW: " IPSTR, IP2STR(&ipInfo.gw));
//     ESP_LOGI(TAG, "Mask: " IPSTR, IP2STR(&ipInfo.netmask));
//     wifi_config_t wifi_config = {
//         .ap = {
//             .ssid = EXAMPLE_ESP_WIFI_SSID,
//             .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
//             .password = EXAMPLE_ESP_WIFI_PASS,
//             .max_connection = EXAMPLE_MAX_STA_CONN,
//             .authmode = WIFI_AUTH_WPA_WPA2_PSK
//         },
//     };
//     if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
//         wifi_config.ap.authmode = WIFI_AUTH_OPEN;
//     }
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//     ap_sta = 0;
// }

// void from_ap_to_sta(void){
// printf("## Entering from_ap_to_sta.\n");
//     if(ap_sta == 0){
//         ESP_ERROR_CHECK(esp_wifi_stop());
//         ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
//     }
//     s_wifi_event_group = xEventGroupCreate();
//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = ESP_WIFI_SSID,
//             .password = ESP_WIFI_PASS,
// 	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//             .pmf_cfg = {
//                 .capable = true,
//                 .required = false
//             },
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE,
//             pdFALSE, portMAX_DELAY);
//     if (bits & WIFI_CONNECTED_BIT) {
//         ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
//     }
//     else if (bits & WIFI_FAIL_BIT) {
//         ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
//     }
//     else {
//         ESP_LOGE(TAG, "UNEXPECTED EVENT");
//     }
//     vEventGroupDelete(s_wifi_event_group);
//     ap_sta = 1;
// }
