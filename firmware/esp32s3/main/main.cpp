#include <math.h>
#include <stdio.h>

#include "sdkconfig.h"

#include "soc/gpio_num.h"

#include "bootloader_random.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ssd1306.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include "Arduino.h"
#include "Wire.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "Adafruit_TSL2591.h"

#include "BSEC3.h"
#include "bme69xLibrary.h"
#include "bsecConfig/bsec_iaq.h"

#include "adafruit_renderer.h"
#include "grid_composer.h"

#include "cbor_sensor_encoder.h"

#include "adc_module.h"
#include "mqtt_module.h"
#include "sntp_module.h"
#include "wifi_module.h"

#define SENSOR_ID "clock1"

#define I2C_SCL_BUS0  GPIO_NUM_1
#define I2C_SDA_BUS0  GPIO_NUM_2
#define I2C_FREQ_BUS0 400000U

#define I2C_SCL_BUS1  GPIO_NUM_3
#define I2C_SDA_BUS1  GPIO_NUM_4
#define I2C_FREQ_BUS1 400000U

#define ADC_SOUND_SENSOR_DATA_PIN            GPIO_NUM_6
#define ADC_SOUND_SENSOR_DATA_BITWIDTH       12U
#define ADC_SOUND_SENSOR_PERFORM_CALIBRATION 1U
#define ADC_SOUND_SENSOR_ADC_SAMPLE_FREQ     2000U
#define ADC_SOUND_SENSOR_READ_SAMPLES        25U
#define ADC_SOUND_SENSOR_DC_OFFSET_MV        1650U
#define ADC_SOUND_SENSOR_TASK_PERIOD_MS      100U
#define ADC_SOUND_SENSOR_CALC_PERIOD_MS      4000U
#define ADC_SOUND_SENSOR_REPORT_PERIOD_MS    4000U

#define TSL2591_TASK_PERIOD_MS   2800U
#define TSL2591_CALC_PERIOD_MS   3600U * 1000U * 12U
#define TSL2591_REPORT_PERIOD_MS 2800U
#define TSL2591_ADDR             0x29U

#define BME690_TASK_PERIOD_MS   (uint32_t)(1000.0f / (BSEC_SAMPLE_RATE_LP * 2.0f))
#define BME690_REPORT_PERIOD_MS (uint32_t)(1000.0f / (BSEC_SAMPLE_RATE_LP * 2.0f))
#define BME690_I2C_ADDRESS      0x76U

#define SH1106_1_IDX          0U
#define SH1106_2_IDX          1U
#define SH1106_3_IDX          2U
#define SH1106_SCREEN_WIDTH   128U
#define SH1106_SCREEN_HEIGHT  64U
#define SH1106_OLED_RESET     -1
#define SH1106_SCREEN_ADDRESS 0x3CU
#define SH1106_PCA1_ADDR      0x70U

#define SNTP_SERVER_1         "sth1.ntp.se"
#define SNTP_SYNC_WAIT_MS     40000U
#define SNTP_TASK_PERIOD_MS   15000U
#define SNTP_REPORT_PERIOD_MS 30000U

#define WIFI_SSID          "COMHEM_6734e0"
#define WIFI_PASS          "qdmmdnwm"
#define WIFI_MAXIMUM_RETRY 5U

#define MQTT_SERVER           "192.168.0.124"
#define MQTT_PORT             1883U
#define MQTT_USERNAME         "esp32_1"
#define MQTT_PASSWORD         "Test12345"
#define MQTT_DATA_OUT_TOPIC   "/IoT-Clock-RoomMonitor/DEVICE_OUT/DATA"
#define MQTT_BUFFER_COUNT     3U
#define MQTT_MAX_MESSAGE_SIZE 2048U

#define DATA_AGGREGATION_MAX_PAYLOADS       15U
#define DATA_AGGREGATION_CBOR_OVERHEAD_COEF 1.2f /* Assuming 20% overhead */

#define ESP_ERR_MAIN_APP_BASE                0x8000U
#define ESP_ERR_MAIN_APP_BME690_FAIL         (ESP_ERR_MAIN_APP_BASE + 1U)
#define ESP_ERR_MAIN_APP_BME690_BSEC_FAIL    (ESP_ERR_MAIN_APP_BASE + 2U)
#define ESP_ERR_MAIN_APP_I2C_FAIL            (ESP_ERR_MAIN_APP_BASE + 3U)
#define ESP_ERR_MAIN_APP_ADC_FAIL            (ESP_ERR_MAIN_APP_BASE + 4U)
#define ESP_ERR_MAIN_APP_WIFI_FAIL           (ESP_ERR_MAIN_APP_BASE + 5U)
#define ESP_ERR_MAIN_APP_MQTT_FAIL           (ESP_ERR_MAIN_APP_BASE + 6U)
#define ESP_ERR_MAIN_APP_SNTP_FAIL           (ESP_ERR_MAIN_APP_BASE + 7U)
#define ESP_ERR_MAIN_APP_GRID_COMPOSER_FAIL  (ESP_ERR_MAIN_APP_BASE + 8U)
#define ESP_ERR_MAIN_APP_SH1106_DISPLAY_FAIL (ESP_ERR_MAIN_APP_BASE + 9U)
#define ESP_ERR_MAIN_APP_TSL2591_FAIL        (ESP_ERR_MAIN_APP_BASE + 10U)

#define TASK_QUEUE_SEND_TIMEOUT_MS 1000U

typedef struct {
  uint8_t buffer[(uint32_t)(MQTT_MAX_MESSAGE_SIZE * DATA_AGGREGATION_CBOR_OVERHEAD_COEF)];
  uint32_t length;
} mqtt_message;

typedef struct {
  adafruit_renderer_ctx renderer_ctx;
  TwoWire *tw;
} draw_wrapper_ctx;

BME69x bme690;
BSEC3 bme690_bsec;

Adafruit_TSL2591 tsl2591;

adc_dev_handle adc_sound_sens;

Adafruit_SH1106G sh1106_1 = Adafruit_SH1106G(SH1106_SCREEN_WIDTH, SH1106_SCREEN_HEIGHT, &Wire, SH1106_OLED_RESET);
Adafruit_SH1106G sh1106_2 = Adafruit_SH1106G(SH1106_SCREEN_WIDTH, SH1106_SCREEN_HEIGHT, &Wire, SH1106_OLED_RESET);
Adafruit_SH1106G sh1106_3 = Adafruit_SH1106G(SH1106_SCREEN_WIDTH, SH1106_SCREEN_HEIGHT, &Wire, SH1106_OLED_RESET);

U8G2_FOR_ADAFRUIT_GFX u8g2_1;
U8G2_FOR_ADAFRUIT_GFX u8g2_2;
U8G2_FOR_ADAFRUIT_GFX u8g2_3;

grid_composer_handle grid_composer;
// Could be malloc'ed but too lazy to manage lifetimes
draw_wrapper_ctx wrap_ctx1;
draw_wrapper_ctx wrap_ctx2;
draw_wrapper_ctx wrap_ctx3;

mqtt_module_handle mqtt_module;

uint64_t boot_to_utc_offset_us;

TaskHandle_t task_sound_sampling_handle;
TaskHandle_t task_bme690_sampling_handle;
TaskHandle_t task_tsl2591_sampling_handle;
TaskHandle_t task_sntp_sampling_handle;

TaskHandle_t task_sensor_data_aggregation_handle;
TaskHandle_t task_mqtt_sending_handle;
TaskHandle_t task_display_snd_lux_handle;

QueueHandle_t data_aggregation_queue_handle;

QueueHandle_t mqtt_free_queue;
QueueHandle_t mqtt_filled_queue;

mqtt_message mqtt_buffers[MQTT_BUFFER_COUNT];

static const char *TAG = "clock_room_monitor_app";

void
pca_select(TwoWire *tw, uint16_t i);
esp_err_t
draw_text_wrapper(void *ctx, const grid_composer_text_info *info, uint16_t color, bool fill);
esp_err_t
draw_figure_wrapper(void *ctx, const grid_composer_figure_info *info, uint16_t color, bool fill);
esp_err_t
clear_wrapper(void *ctx);

esp_err_t
init_i2c();

esp_err_t
init_bme690();
esp_err_t
init_bsec();
esp_err_t
init_adc_sound_sensor();
esp_err_t
init_tsl2591();
esp_err_t
init_sh1106_n();
esp_err_t
init_grid();

esp_err_t
init_wifi();
esp_err_t
init_sntp();
esp_err_t
init_mqtt();

void
task_sound_sampling(void *arg);
void
task_bme690_sampling(void *arg);
void
task_tsl2591_sampling(void *arg);
void
task_sntp_sampling(void *arg);

void
task_sensor_data_aggregation(void *arg);

void
task_mqtt_sending(void *arg);
esp_err_t
mqtt_data_event_handler(mqtt_module_handle mqtt_module, int32_t event_id, esp_mqtt_event_handle_t event_handle);

extern "C" void
app_main() {
  initArduino();
  vTaskDelay(pdMS_TO_TICKS(1000));

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK_WITHOUT_ABORT(ret);

  ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_loop_create_default());

  vTaskDelay(pdMS_TO_TICKS(1000));

  uint32_t task_jitter_random[3];

  bootloader_random_enable();
  esp_fill_random(task_jitter_random, 3U);
  bootloader_random_disable();

  init_i2c();

  init_bme690();
  init_bsec();

  init_tsl2591();

  init_adc_sound_sensor();

  init_sh1106_n();
  init_grid();

  init_wifi();

  vTaskDelay(pdMS_TO_TICKS(3000));

  init_sntp();
  init_mqtt();

  data_aggregation_queue_handle = xQueueCreate(20U, sizeof(sensor_payload_t));
  if (NULL == data_aggregation_queue_handle) {
    ESP_LOGE(TAG, "Failed to initialize data_aggregation_queue");
    return;
  }

  mqtt_free_queue = xQueueCreate(MQTT_BUFFER_COUNT, sizeof(mqtt_message *));
  mqtt_filled_queue = xQueueCreate(MQTT_BUFFER_COUNT, sizeof(mqtt_message *));

  for (int i = 0; i < MQTT_BUFFER_COUNT; ++i) {
    mqtt_message *msg = &mqtt_buffers[i];
    xQueueSend(mqtt_free_queue, &msg, 0);
  }

  xTaskCreatePinnedToCore(task_mqtt_sending, "mqtt tsk", 8196U, NULL, 9, &task_mqtt_sending_handle, 1U);
  xTaskCreatePinnedToCore(task_sensor_data_aggregation, "aggr tsk", 6144U, NULL, 8, &task_sensor_data_aggregation_handle, 0U);

  xTaskCreatePinnedToCore(task_bme690_sampling, "bme690 tsk", 3144U, NULL, 6, &task_bme690_sampling_handle, 0U);
  vTaskDelay(pdMS_TO_TICKS(10U * (task_jitter_random[0] & 0x00000001)));
  xTaskCreatePinnedToCore(task_tsl2591_sampling, "tsl2591 tsk", 3144U, NULL, 6, &task_tsl2591_sampling_handle, 0U);
  vTaskDelay(pdMS_TO_TICKS(10U * (task_jitter_random[1] & 0x00000001)));
  xTaskCreatePinnedToCore(task_sound_sampling, "sound tsk", 3144U, NULL, 6, &task_sound_sampling_handle, 0U);
  vTaskDelay(pdMS_TO_TICKS(10U * (task_jitter_random[2] & 0x00000001)));
  xTaskCreatePinnedToCore(task_sntp_sampling, "sntp tsk", 3144U, NULL, 6, &task_sntp_sampling_handle, 0U);

  for (;;) {
    char *task_buffer = (char *)pvPortMalloc(2048);
    if (task_buffer == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for task list");
      return;
    }

    vTaskList(task_buffer);
    ESP_LOGI(TAG, "\nTask Name\tState\tPrio\tStack\tNum\n%s", task_buffer);

    vPortFree(task_buffer);

    vTaskDelay(pdMS_TO_TICKS(4000U));

    // grid_composer_draw_descriptor draw1 = {};

    // draw1.cell_row = 0;
    // draw1.cell_col = 0;
    // draw1.draw_obj.fill = false;
    // draw1.draw_obj.color = SH110X_WHITE;
    // draw1.draw_obj.content_type = GRID_COMPOSER_CONTENT_TEXT;
    // draw1.draw_obj.text.font = u8g2_font_helvR24_tn;
    // draw1.draw_obj.text.h_align = GRID_COMPOSER_H_ALIGN_LEFT;
    // draw1.draw_obj.text.v_align = GRID_COMPOSER_V_ALIGN_TOP;
    // draw1.draw_obj.text.text = "456";
    // ESP_ERROR_CHECK_WITHOUT_ABORT(grid_composer_draw_queue_send(grid_composer, &draw1, 10000));
  }
}

esp_err_t
init_i2c() {
  esp_err_t ret = ESP_OK;

  Wire.end();

  ESP_RETURN_ON_FALSE(Wire.begin(I2C_SDA_BUS0, I2C_SCL_BUS0, I2C_FREQ_BUS0), ESP_ERR_MAIN_APP_I2C_FAIL, TAG,
                      "Failed to initialize I2C bus 0");

  ESP_RETURN_ON_FALSE(Wire1.begin(I2C_SDA_BUS1, I2C_SCL_BUS1, I2C_FREQ_BUS1), ESP_ERR_MAIN_APP_I2C_FAIL, TAG,
                      "Failed to initialize I2C bus 1");

  ESP_LOGI(TAG, "Initialized I2C");
  return ret;
}

esp_err_t
init_bsec() {
  esp_err_t ret = ESP_OK;

  ESP_RETURN_ON_FALSE(bme690_bsec.begin(bme690, 0), ESP_ERR_MAIN_APP_BME690_BSEC_FAIL, TAG,
                      "Failed to initialize BSEC instance, code: %i", bme690_bsec.status);

  bme690_bsec.setTemperatureOffset(BSEC_SAMPLE_RATE_LP);
  bme690_bsec.setTVOCBaselineCalibration(false);

  ESP_RETURN_ON_FALSE(bme690_bsec.setConfig(bsec_config_iaq), ESP_ERR_MAIN_APP_BME690_BSEC_FAIL, TAG,
                      "Failed to load BSEC config file, code: %i", bme690_bsec.status);

  bsecVirtualSensor bme690_bsec_sensor_outputs[] = {
      BSEC_OUTPUT_IAQ,

      BSEC_OUTPUT_RAW_PRESSURE,

      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
      BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
  };

  ESP_RETURN_ON_FALSE(
      bme690_bsec.updateSubscription(bme690_bsec_sensor_outputs, (sizeof(bme690_bsec_sensor_outputs) / sizeof(bsecVirtualSensor)),
                                     BSEC_SAMPLE_RATE_LP),
      ESP_ERR_MAIN_APP_BME690_BSEC_FAIL, TAG, "Failed to update BSEC output subscriptions, code: %i", bme690_bsec.status);

  ESP_LOGI(TAG, "Initialized BSEC");

  return ret;
}
esp_err_t
init_bme690() {
  esp_err_t ret = ESP_OK;

  bme690.begin(BME690_I2C_ADDRESS, Wire);
  ESP_RETURN_ON_FALSE(bme690.status == BME69X_OK, ESP_ERR_MAIN_APP_BME690_FAIL, TAG,
                      "Failed to initialize BME690 sensor, code: %i", bme690.status);

  ESP_LOGI(TAG, "Initialized BME690");

  return ret;
}
esp_err_t
init_adc_sound_sensor() {
  esp_err_t ret = ESP_OK;

  adc_base_dev_config_t adc_dev_cfg = {
      .data_pin = ADC_SOUND_SENSOR_DATA_PIN,
      .data_bit_width = ADC_SOUND_SENSOR_DATA_BITWIDTH,
      .perform_calibration = ADC_SOUND_SENSOR_PERFORM_CALIBRATION,
  };
  adc_dev_cont_config_t adc_cont_cfg = {
      .sample_freq_hz = ADC_SOUND_SENSOR_ADC_SAMPLE_FREQ,
  };
  ESP_RETURN_ON_ERROR(adc_module_init_dev_continuous(&adc_dev_cfg, &adc_cont_cfg, &adc_sound_sens), TAG,
                      "Failed to initialize an adc device");
  ESP_RETURN_ON_ERROR(adc_module_enable_dev(adc_sound_sens), TAG, "Failed to enable an adc device");
  ESP_LOGI(TAG, "Initialized an analogue sound sensor");
  return ret;
}
esp_err_t
init_tsl2591() {
  esp_err_t ret = ESP_OK;
  ESP_RETURN_ON_FALSE(tsl2591.begin(&Wire, TSL2591_ADDR), ESP_ERR_MAIN_APP_TSL2591_FAIL, TAG,
                      "Failed to initialize TSL2591 sensor");
  tsl2591.setGain(TSL2591_GAIN_HIGH);
  tsl2591.setTiming(TSL2591_INTEGRATIONTIME_400MS);
  ESP_LOGI(TAG, "Initialized TSL2591 sensor");
  return ret;
}
esp_err_t
init_sh1106_n() {
  esp_err_t ret = ESP_OK;

  pca_select(&Wire, SH1106_1_IDX);
  ESP_RETURN_ON_FALSE(sh1106_1.begin(SH1106_SCREEN_ADDRESS, true), ESP_ERR_MAIN_APP_SH1106_DISPLAY_FAIL, TAG,
                      "Failed to initialize SH1106 num 1");
  sh1106_1.clearDisplay();
  sh1106_1.display();
  ESP_LOGI(TAG, "Initialized SH1106 num 1");

  pca_select(&Wire, SH1106_2_IDX);
  ESP_RETURN_ON_FALSE(sh1106_2.begin(SH1106_SCREEN_ADDRESS, true), ESP_ERR_MAIN_APP_SH1106_DISPLAY_FAIL, TAG,
                      "Failed to initialize SH1106 num 2");
  sh1106_2.clearDisplay();
  sh1106_2.display();
  ESP_LOGI(TAG, "Initialized SH1106 num 2");

  pca_select(&Wire, SH1106_3_IDX);
  ESP_RETURN_ON_FALSE(sh1106_3.begin(SH1106_SCREEN_ADDRESS, true), ESP_ERR_MAIN_APP_SH1106_DISPLAY_FAIL, TAG,
                      "Failed to initialize SH1106 num 3");
  sh1106_2.clearDisplay();
  sh1106_2.display();
  ESP_LOGI(TAG, "Initialized SH1106 num 3");

  u8g2_1.begin(sh1106_1);
  ESP_LOGI(TAG, "Initialized u8g2 for SH1106 num 1");

  u8g2_2.begin(sh1106_2);
  ESP_LOGI(TAG, "Initialized u8g2 for SH1106 num 2");

  u8g2_3.begin(sh1106_3);
  ESP_LOGI(TAG, "Initialized u8g2 for SH1106 num 3");

  return ret;
}
esp_err_t
init_grid() {
  esp_err_t ret = ESP_OK;

  wrap_ctx1 = {
      .renderer_ctx =
          {
              .oled = &sh1106_1,
              .u8g2 = &u8g2_1,
              .idx = SH1106_1_IDX,
          },
  };
  wrap_ctx1.tw = &Wire;
  grid_composer_renderer renderer1 = {
      .user_ctx = &wrap_ctx1,
      .draw_text = draw_text_wrapper,
      .draw_figure = draw_figure_wrapper,
      .clear = clear_wrapper,
  };

  wrap_ctx2 = {
      .renderer_ctx =
          {
              .oled = &sh1106_2,
              .u8g2 = &u8g2_2,
              .idx = SH1106_2_IDX,
          },
  };
  wrap_ctx2.tw = &Wire;
  grid_composer_renderer renderer2 = {
      .user_ctx = &wrap_ctx2,
      .draw_text = draw_text_wrapper,
      .draw_figure = draw_figure_wrapper,
      .clear = clear_wrapper,
  };

  wrap_ctx3 = {
      .renderer_ctx =
          {
              .oled = &sh1106_3,
              .u8g2 = &u8g2_3,
              .idx = SH1106_3_IDX,
          },
  };
  wrap_ctx3.tw = &Wire;
  grid_composer_renderer renderer3 = {
      .user_ctx = &wrap_ctx3,
      .draw_text = draw_text_wrapper,
      .draw_figure = draw_figure_wrapper,
      .clear = clear_wrapper,
  };

  ESP_RETURN_ON_ERROR(grid_composer_init(&grid_composer), TAG, "Failed to initialize grid composer");

  ESP_RETURN_ON_ERROR(grid_composer_add_cell(grid_composer, &renderer1), TAG, "Failed to add a cell device 1");
  ESP_RETURN_ON_ERROR(grid_composer_add_cell(grid_composer, &renderer2), TAG, "Failed to add a cell device 2");
  ESP_RETURN_ON_ERROR(grid_composer_add_cell(grid_composer, &renderer3), TAG, "Falied to add a cell device 3");

  return ret;
}

esp_err_t
init_wifi() {
  ESP_RETURN_ON_ERROR(wifi_module_init(), TAG, "Failed to initialize wifi module");
  struct wifi_module_config module_cfg = {
      .sta =
          {
              .ssid = WIFI_SSID,
              .password = WIFI_PASS,
              .auth_mode = WIFI_AUTH_WPA2_PSK,
              .max_retries = 10,
              .retry_delay_ms = 1000,
          },
  };
  ESP_RETURN_ON_ERROR(wifi_module_enable_mode(WIFI_MODULE_MODE_STA, &module_cfg), TAG,
                      "Failed to enable STA mode for wifi module");
  ESP_RETURN_ON_ERROR(wifi_module_start(), TAG, "Failed to start wifi module");
  return ESP_OK;
}

esp_err_t
init_sntp() {
  esp_err_t ret = ESP_OK;

  sntp_module_config sntp_cfg = {
      .server = SNTP_SERVER_1,
  };
  ESP_RETURN_ON_ERROR(sntp_module_init(&sntp_cfg), TAG, "Failed to initialize SNTP module");
  ESP_RETURN_ON_ERROR(sntp_module_start(), TAG, "Failed to start SNTP module");
  ESP_RETURN_ON_ERROR(sntp_module_sync_wait(SNTP_SYNC_WAIT_MS), TAG, "Failed to sync SNTP module");

  struct timeval tv_now;
  gettimeofday(&tv_now, NULL);
  uint64_t sntp_synced_utc_us = (uint64_t)tv_now.tv_sec * 1000000L + (uint64_t)tv_now.tv_usec;
  uint64_t boot_time_us = esp_timer_get_time();

  boot_to_utc_offset_us = sntp_synced_utc_us - boot_time_us;

  // Could be made configurable
  setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
  tzset();

  ESP_LOGI(TAG, "Initialized SNTP module");
  return ret;
}
esp_err_t
init_mqtt() {
  esp_err_t ret = ESP_OK;

  mqtt_module_init(&mqtt_module);
  struct mqtt_address_config addr_cfg = {
      .hostname = MQTT_SERVER,
      .transport = MQTT_TRANSPORT_OVER_TCP,
      .path = "/",
      .port = MQTT_PORT,
  };

  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_register_event_handler(mqtt_module, MQTT_MODULE_DATA_EVENT, mqtt_data_event_handler));

  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_set_address_cfg(mqtt_module, &addr_cfg));
  struct mqtt_credentials_config cred_cfg = {
      .username = MQTT_USERNAME,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_set_credentials_cfg(mqtt_module, &cred_cfg));
  struct mqtt_auth_config auth_cfg = {
      .password = MQTT_PASSWORD,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_set_authentication_cfg(mqtt_module, &auth_cfg));
  struct mqtt_session_config sesh_cfg = {
      .disable_clean_session = true,
  };
  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_set_session_cfg(mqtt_module, &sesh_cfg));
  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_connect(mqtt_module, 15000));
  ESP_ERROR_CHECK_WITHOUT_ABORT(mqtt_module_subscribe(mqtt_module, "/IoT-Clock-RoomMonitor/DEVICE_IN/CTRL", 0));

  ESP_LOGI(TAG, "Initialized MQTT module");
  return ret;
}

void
task_sound_sampling(void *arg) {
  sensor_payload_t payload;
  strncpy(payload.sensor, "sound_sens", SENSOR_NAME_MAX_LEN - 1);
  payload.sensor[SENSOR_NAME_MAX_LEN - 1] = '\0';

  uint32_t adc_data[ADC_SOUND_SENSOR_READ_SAMPLES];

  uint32_t samples_read = 0U;

  float rms_value = 0.0f;
  float rms_max_value = 0.0f;
  float rms_min_value = 0.0f;

  uint32_t accumulator_value = 0ULL;
  uint32_t values_count = 0ULL;

  uint64_t prev_calc_timestamp_us = 0ULL;
  uint64_t prev_report_timestamp_us = 0ULL;

  uint64_t calc_period_us = ADC_SOUND_SENSOR_CALC_PERIOD_MS * 1000ULL;
  uint64_t report_period_us = ADC_SOUND_SENSOR_REPORT_PERIOD_MS * 1000ULL;

  uint64_t delay_ms = (uint64_t)ADC_SOUND_SENSOR_TASK_PERIOD_MS;
  for (;;) {
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_module_get_data(adc_sound_sens, ADC_SOUND_SENSOR_READ_SAMPLES, adc_data, &samples_read));

    for (uint16_t i = 0; i < samples_read; ++i) {
      int v = adc_data[i] - ADC_SOUND_SENSOR_DC_OFFSET_MV;
      accumulator_value += v * v;
    }
    values_count += samples_read;

    uint64_t curr_timestamp_us = (uint64_t)esp_timer_get_time();
    if ((curr_timestamp_us - prev_calc_timestamp_us) >= calc_period_us) {
      prev_calc_timestamp_us = curr_timestamp_us;

      rms_value = (values_count) ? sqrt(accumulator_value / values_count) : 0.0f;
      samples_read = 0U;
      accumulator_value = 0U;
      values_count = 0U;

      rms_max_value = max(rms_value, rms_max_value);
      rms_min_value = min(rms_value, rms_min_value);
    }

    if ((curr_timestamp_us - prev_report_timestamp_us) >= report_period_us) {
      prev_report_timestamp_us = curr_timestamp_us;

      payload.timestamp = (uint64_t)(curr_timestamp_us + boot_to_utc_offset_us);

      strncpy(payload.fields[0].name, "rms_max_sound", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[0].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[0].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[0].value.f = rms_max_value;

      strncpy(payload.fields[1].name, "rms_min_sound", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[1].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[1].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[1].value.f = rms_min_value;

      strncpy(payload.fields[2].name, "rms_sound", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[2].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[2].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[2].value.f = rms_value;

      strncpy(payload.fields[3].name, "win_s", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[3].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[3].type = SENSOR_FIELD_DATATYPE_UINT;
      payload.fields[3].value.u = (uint64_t)(ADC_SOUND_SENSOR_REPORT_PERIOD_MS / 1000ULL);

      payload.field_count = 4U;

      if (xQueueSend(data_aggregation_queue_handle, &payload, pdMS_TO_TICKS(TASK_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed sending from sound sensor task");
      } else {
        ESP_LOGI(TAG, "Sent data from sound sensor task");
      }
      // Send to display_data queue
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}
void
task_tsl2591_sampling(void *arg) {
  sensor_payload_t payload;

  strncpy(payload.sensor, "tsl2591", SENSOR_NAME_MAX_LEN - 1);
  payload.sensor[SENSOR_NAME_MAX_LEN - 1] = '\0';

  float max_lux = 0.0f;
  float min_lux = 0.0f;

  uint64_t prev_calc_timestamp_us = 0ULL;
  uint64_t prev_report_timestamp_us = 0ULL;

  uint64_t calc_period_us = (uint64_t)(TSL2591_CALC_PERIOD_MS * 1000ULL);
  uint64_t report_period_us = (uint64_t)(TSL2591_REPORT_PERIOD_MS * 1000ULL);

  uint64_t delay_ms = (uint64_t)(TSL2591_TASK_PERIOD_MS);
  for (;;) {
    uint32_t lum = tsl2591.getFullLuminosity();

    uint32_t ir = lum >> 16;
    uint32_t full = lum & 0xFFFF;

    float lux = tsl2591.calculateLux(full, ir);

    uint64_t curr_timestamp_us = (uint64_t)esp_timer_get_time();
    if ((curr_timestamp_us - prev_calc_timestamp_us) >= calc_period_us) {
      prev_calc_timestamp_us = curr_timestamp_us;

      max_lux = max(lux, max_lux);
      min_lux = min(lux, min_lux);
    }

    if ((curr_timestamp_us - prev_report_timestamp_us) >= report_period_us) {
      prev_report_timestamp_us = curr_timestamp_us;

      payload.timestamp = (uint64_t)(curr_timestamp_us + boot_to_utc_offset_us);

      strncpy(payload.fields[0].name, "max_lux", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[0].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[0].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[0].value.f = max_lux;

      strncpy(payload.fields[1].name, "min_lux", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[1].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[1].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[1].value.f = min_lux;

      strncpy(payload.fields[2].name, "lux", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[2].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[2].type = SENSOR_FIELD_DATATYPE_FLOAT;
      payload.fields[2].value.f = lux;

      strncpy(payload.fields[3].name, "win_s", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[3].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[3].type = SENSOR_FIELD_DATATYPE_UINT;
      payload.fields[3].value.u = (uint64_t)(TSL2591_CALC_PERIOD_MS / 1000ULL);

      payload.field_count = 4U;

      if (xQueueSend(data_aggregation_queue_handle, &payload, pdMS_TO_TICKS(TASK_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed sending from tsl2591 task");
      } else {
        ESP_LOGI(TAG, "Sent data from tsl2591 task");
      }
    }

    // Send to display_data queue

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}
void
task_bme690_sampling(void *arg) {
  sensor_payload_t payload;
  strncpy(payload.sensor, "bme690", SENSOR_NAME_MAX_LEN - 1);
  payload.sensor[SENSOR_NAME_MAX_LEN - 1] = '\0';

  char humid_text[32];
  char temp_text[32];
  char iaq_text[32];
  float temp = 0.0f;
  float humid = 0.0f;
  float iaq = 0.0f;

  uint64_t prev_report_timestamp_us = 0ULL;

  uint64_t report_period_us = BME690_REPORT_PERIOD_MS * 1000ULL;

  uint64_t delay_ms = (uint64_t)BME690_TASK_PERIOD_MS;
  for (;;) {
    const bsecOutputs *outputs;
    if (!bme690_bsec.run()) {
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
      continue;
    }

    outputs = bme690_bsec.getBSECOutputs();

    if (!outputs || outputs->nOutputChnls == 0) {
      vTaskDelay(pdMS_TO_TICKS(delay_ms));
      continue;
    }
    size_t field_index = 0;
    for (uint8_t i = 0; i < outputs->nOutputChnls; i++) {
      if (field_index >= SENSOR_MAX_FIELDS)
        break;

      const char *value_name = NULL;
      sensor_field_datatype_t value_type = SENSOR_FIELD_DATATYPE_INVALID;

      switch (outputs->outputChnls[i].sensor_id) {
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
        value_name = "temp";
        value_type = SENSOR_FIELD_DATATYPE_FLOAT;
        payload.fields[field_index].value.f = outputs->outputChnls[i].signal;

        temp = outputs->outputChnls[i].signal;
        break;
      case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
        value_name = "humid";
        value_type = SENSOR_FIELD_DATATYPE_FLOAT;
        payload.fields[field_index].value.f = outputs->outputChnls[i].signal;

        humid = outputs->outputChnls[i].signal;
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
        value_name = "press";
        value_type = SENSOR_FIELD_DATATYPE_FLOAT;
        payload.fields[field_index].value.f = outputs->outputChnls[i].signal;
        break;
      case BSEC_OUTPUT_IAQ:
        value_name = "iaq";
        value_type = SENSOR_FIELD_DATATYPE_FLOAT;
        payload.fields[field_index].value.f = outputs->outputChnls[i].signal;

        iaq = outputs->outputChnls[i].signal;
        break;
      default:
        value_type = SENSOR_FIELD_DATATYPE_INVALID;
        continue;
      }

      if (value_name && value_type != SENSOR_FIELD_DATATYPE_INVALID) {
        payload.fields[field_index].type = value_type;
        strncpy(payload.fields[field_index].name, value_name, SENSOR_FIELD_NAME_LEN - 1);
        payload.fields[field_index].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
        field_index++;
      }
    }

    payload.field_count = field_index;
    uint64_t curr_timestamp_us = (uint64_t)esp_timer_get_time();
    if ((curr_timestamp_us - prev_report_timestamp_us) >= report_period_us) {
      prev_report_timestamp_us = curr_timestamp_us;

      payload.timestamp = (uint64_t)(curr_timestamp_us + boot_to_utc_offset_us);

      if (field_index) {
        if (xQueueSend(data_aggregation_queue_handle, &payload, pdMS_TO_TICKS(TASK_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
          ESP_LOGE(TAG, "Failed sending from bme690 task");
        } else {
          ESP_LOGI(TAG, "Sent data from bme690 task");
        }
      }

      snprintf(temp_text, sizeof(temp_text), "Temperature:%.1f", temp);
      grid_composer_draw_descriptor draw_desc_temp = GRID_COMPOSER_TEXT_DESCRIPTOR(
          0, 1, 0, 0, GRID_COMPOSER_V_ALIGN_TOP, GRID_COMPOSER_H_ALIGN_LEFT, temp_text, u8g2_font_7x14_tf, SH110X_WHITE, true);
      ESP_ERROR_CHECK_WITHOUT_ABORT(grid_composer_draw_queue_send(grid_composer, &draw_desc_temp, 1000U));
      // memset(t_text, 0, sizeof(t_text));

      snprintf(humid_text, sizeof(humid_text), "Humidity:%.1f", humid);
      grid_composer_draw_descriptor draw_desc_humid =
          GRID_COMPOSER_TEXT_DESCRIPTOR(0, 1, 0, 24, GRID_COMPOSER_V_ALIGN_CENTER, GRID_COMPOSER_H_ALIGN_LEFT, humid_text,
                                        u8g2_font_7x14_tf, SH110X_WHITE, false);
      ESP_ERROR_CHECK_WITHOUT_ABORT(grid_composer_draw_queue_send(grid_composer, &draw_desc_humid, 1000U));
      // memset(t_text, 0, sizeof(t_text));

      snprintf(iaq_text, sizeof(iaq_text), "IAQ Index:%.1f", iaq);
      grid_composer_draw_descriptor draw_desc_iaq =
          GRID_COMPOSER_TEXT_DESCRIPTOR(0, 1, 0, 48, GRID_COMPOSER_V_ALIGN_BOTTOM, GRID_COMPOSER_H_ALIGN_LEFT, iaq_text,
                                        u8g2_font_7x14_tf, SH110X_WHITE, false);

      ESP_ERROR_CHECK_WITHOUT_ABORT(grid_composer_draw_queue_send(grid_composer, &draw_desc_iaq, 1000U));
      // memset(t_text, 0, sizeof(t_text));
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}
void
task_sntp_sampling(void *arg) {
  sensor_payload_t payload;
  strncpy(payload.sensor, "sntp", SENSOR_NAME_MAX_LEN - 1);
  payload.sensor[SENSOR_NAME_MAX_LEN - 1] = '\0';

  time_t now;
  tm timeinfo;
  char t_text[32];
  bool show_colon = false;

  uint64_t prev_report_timestamp_us = 0ULL;
  uint64_t report_period_us = SNTP_REPORT_PERIOD_MS * 1000ULL;

  uint64_t delay_ms = (uint64_t)(SNTP_TASK_PERIOD_MS);
  for (;;) {
    time(&now);
    localtime_r(&now, &timeinfo);

    uint64_t curr_timestamp_us = (uint64_t)esp_timer_get_time();
    if ((curr_timestamp_us - prev_report_timestamp_us) >= report_period_us) {
      prev_report_timestamp_us = curr_timestamp_us;

      strncpy(payload.fields[0].name, "sntp_time", SENSOR_FIELD_NAME_LEN - 1);
      payload.fields[0].name[SENSOR_FIELD_NAME_LEN - 1] = '\0';
      payload.fields[0].type = SENSOR_FIELD_DATATYPE_LONG_UINT;
      payload.fields[0].value.u = (uint64_t)((curr_timestamp_us + boot_to_utc_offset_us) / 1000000ULL);

      payload.timestamp = (uint64_t)(curr_timestamp_us + boot_to_utc_offset_us);

      payload.field_count = 1U;

      if (xQueueSend(data_aggregation_queue_handle, &payload, pdMS_TO_TICKS(TASK_QUEUE_SEND_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed sending from sntp task");
      } else {
        ESP_LOGI(TAG, "Sent data from sntp task");
      }

      sprintf(t_text, "%02i:%02i", timeinfo.tm_hour, timeinfo.tm_min);
      grid_composer_draw_descriptor draw_desc =
          GRID_COMPOSER_TEXT_DESCRIPTOR(0U, 0U, 0U, 0U, GRID_COMPOSER_V_ALIGN_CENTER, GRID_COMPOSER_H_ALIGN_CENTER, t_text,
                                        u8g2_font_10x20_tn, SH110X_WHITE, true);
      ESP_ERROR_CHECK_WITHOUT_ABORT(grid_composer_draw_queue_send(grid_composer, &draw_desc, 1000U));
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

void
task_sensor_data_aggregation(void *arg) {
  sensor_payload_t payloads[DATA_AGGREGATION_MAX_PAYLOADS];

  for (;;) {
    uint32_t payloads_received = 0U;
    mqtt_message *msg = NULL;
    if (xQueueReceive(mqtt_free_queue, &msg, portMAX_DELAY) == pdTRUE) {
      CborEncoder encoder, map_outer, map_data, sensors_array;

      cbor_encoder_init(&encoder, msg->buffer, sizeof(msg->buffer), 0);
      cbor_encoder_create_map(&encoder, &map_outer, CborIndefiniteLength);

      cbor_encode_text_stringz(&map_outer, "data");

      cbor_encoder_create_map(&map_outer, &map_data, CborIndefiniteLength);
      cbor_encode_text_stringz(&map_data, "deviceId");
      cbor_encode_text_stringz(&map_data, SENSOR_ID);

      cbor_encode_text_stringz(&map_data, "sensor_data");

      cbor_encoder_create_array(&map_data, &sensors_array, DATA_AGGREGATION_MAX_PAYLOADS);

      while (payloads_received < DATA_AGGREGATION_MAX_PAYLOADS &&
             (xQueueReceive(data_aggregation_queue_handle, &payloads[payloads_received], portMAX_DELAY) == pdTRUE)) {
        ESP_RETURN_VOID_ON_ERROR(encode_sensor_payload(&payloads[payloads_received], &sensors_array), TAG,
                                 "Failed to encode a sensor payload");
        payloads_received++;
      }

      cbor_encoder_close_container(&map_data, &sensors_array);
      cbor_encoder_close_container(&map_outer, &map_data);
      cbor_encoder_close_container(&encoder, &map_outer);

      uint32_t encoded_len = cbor_encoder_get_buffer_size(&encoder, msg->buffer);
      msg->length = encoded_len;

      ESP_LOGI(TAG, "Encoded an mqtt payload, length:%lu, sensor payloads:%lu", encoded_len, payloads_received);

      if (xQueueSend(mqtt_filled_queue, &msg, pdMS_TO_TICKS(1000U)) != pdTRUE) {
        ESP_LOGW(TAG, "Filled queue full, dropping msg");
        memset(msg->buffer, 0, sizeof(msg->buffer));
        xQueueSend(mqtt_free_queue, &msg, 0);
      }
    }
  }
}

void
task_mqtt_sending(void *arg) {
  esp_err_t ret = ESP_OK;
  mqtt_message *msg = NULL;
  for (;;) {
    if (xQueueReceive(mqtt_filled_queue, &msg, portMAX_DELAY) == pdTRUE) {
      ret = mqtt_module_publish(mqtt_module, MQTT_DATA_OUT_TOPIC, (char *)msg->buffer, msg->length, 0, 0);
      if (ESP_ERR_INVALID_STATE == ret) {
        // wifi_module_stop();
        // wifi_module_start();
        // mqtt_module_del(mqtt_module);
        // init_mqtt();

        mqtt_module_connect(mqtt_module, (uint32_t)portMAX_DELAY);
      }

      memset(msg->buffer, 0, sizeof(msg->buffer));

      xQueueSend(mqtt_free_queue, &msg, 0);
    }
  }
}
esp_err_t
mqtt_data_event_handler(mqtt_module_handle mqtt_module, int32_t event_id, esp_mqtt_event_handle_t event_handle) {
  //   - msg_id               message id
  //   - topic                pointer to the received topic
  //   - topic_len            length of the topic
  //   - data                 pointer to the received data
  //   - data_len             length of the data for this event
  //   - current_data_offset  offset of the current data for this event
  //   - total_data_len       total length of the data received
  //   - retain               retain flag of the message
  //   - qos                  QoS level of the message
  //   - dup                  dup flag of the message
  //   Note: Multiple MQTT_EVENT_DATA could be fired for one
  // message, if it is longer than internal buffer. In that
  // case only first event contains topic pointer and length,
  // other contain data only with current data length and
  // current data offset updating.
  esp_err_t ret = ESP_OK;
  int msg_id = event_handle->msg_id;

  if (msg_id < 0) {
    ESP_LOGE(TAG, "Error receiving a message on %s; Error: %i", event_handle->topic_len ? event_handle->topic : "None", msg_id);
    return ret;
  }

  ESP_LOGI(TAG, "Received on %.*s; Content: %.*s", event_handle->topic_len, event_handle->topic, event_handle->data_len,
           event_handle->data);
  return ret;
}

void
pca_select(TwoWire *tw, uint16_t i) {
  if (i > 7)
    return;

  tw->beginTransmission(SH1106_PCA1_ADDR);
  tw->write(1 << i);
  tw->endTransmission();
}
esp_err_t
draw_text_wrapper(void *ctx, const grid_composer_text_info *info, uint16_t color, bool fill) {
  draw_wrapper_ctx *wrap_ctx = (draw_wrapper_ctx *)ctx;
  adafruit_renderer_ctx *gfx_ctx = &wrap_ctx->renderer_ctx;

  pca_select(wrap_ctx->tw, wrap_ctx->renderer_ctx.idx);

  return adafruit_gfx_draw_text(gfx_ctx, info, color, fill);
}
esp_err_t
draw_figure_wrapper(void *ctx, const grid_composer_figure_info *info, uint16_t color, bool fill) {
  draw_wrapper_ctx *wrap_ctx = (draw_wrapper_ctx *)ctx;
  adafruit_renderer_ctx *gfx_ctx = &wrap_ctx->renderer_ctx;

  pca_select(wrap_ctx->tw, wrap_ctx->renderer_ctx.idx);
  return adafruit_gfx_draw_figure(gfx_ctx, info, color, fill);
}
esp_err_t
clear_wrapper(void *ctx) {
  draw_wrapper_ctx *wrap_ctx = (draw_wrapper_ctx *)ctx;
  adafruit_renderer_ctx *gfx_ctx = &wrap_ctx->renderer_ctx;

  pca_select(wrap_ctx->tw, wrap_ctx->renderer_ctx.idx);
  return adafruit_gfx_clear(gfx_ctx);
}
