#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_log.h"

#include "wifi.h"
#include "ota.h"
#include "leds.h"
#include "app_config.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (wifi_init_sta(WIFI_SSID, WIFI_PASS) != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed");
        return;
    }

    leds_init();
    leds_set_pwm1_quarter();
    leds_start_tasks();

    xTaskCreate(ota_task, "ota_task", 8192, NULL, 5, NULL);
}