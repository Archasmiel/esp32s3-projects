#include "ota.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "cJSON.h"

#include "app_config.h"

static const char *TAG = "ota";

static esp_err_t http_get_text(const char *url, char *buffer, size_t buffer_size);
static int compare_versions(const char *a, const char *b);
static esp_err_t perform_http_ota(const char *firmware_url);

static esp_err_t http_get_text(const char *url, char *buffer, size_t buffer_size)
{
    if (buffer == NULL || buffer_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    buffer[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    ESP_LOGI(TAG, "HTTP status: %d, content_length: %d", status, content_length);

    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    size_t total_read = 0;

    while (total_read < buffer_size - 1) {
        int read_len = esp_http_client_read(
            client,
            buffer + total_read,
            buffer_size - 1 - total_read
        );

        if (read_len < 0) {
            ESP_LOGE(TAG, "HTTP read failed");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (read_len == 0) {
            break;
        }

        total_read += read_len;
    }

    buffer[total_read] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Read %u bytes", (unsigned)total_read);
    ESP_LOGI(TAG, "Response body: %s", buffer);

    return (total_read > 0) ? ESP_OK : ESP_FAIL;
}

static int compare_versions(const char *a, const char *b)
{
    char a_copy[32];
    char b_copy[32];

    strncpy(a_copy, a, sizeof(a_copy) - 1);
    strncpy(b_copy, b, sizeof(b_copy) - 1);
    a_copy[sizeof(a_copy) - 1] = '\0';
    b_copy[sizeof(b_copy) - 1] = '\0';

    char *a_ctx = NULL;
    char *b_ctx = NULL;
    char *a_tok = strtok_r(a_copy, ".", &a_ctx);
    char *b_tok = strtok_r(b_copy, ".", &b_ctx);

    while (a_tok != NULL || b_tok != NULL) {
        int a_num = (a_tok != NULL) ? atoi(a_tok) : 0;
        int b_num = (b_tok != NULL) ? atoi(b_tok) : 0;

        if (a_num != b_num) {
            return a_num - b_num;
        }

        a_tok = (a_tok != NULL) ? strtok_r(NULL, ".", &a_ctx) : NULL;
        b_tok = (b_tok != NULL) ? strtok_r(NULL, ".", &b_ctx) : NULL;
    }

    return 0;
}

static esp_err_t perform_http_ota(const char *firmware_url)
{
    esp_http_client_config_t config = {
        .url = firmware_url,
        .timeout_ms = 15000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to init firmware HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open firmware URL: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int headers_result = esp_http_client_fetch_headers(client);
    if (headers_result < 0) {
        ESP_LOGE(TAG, "Failed to fetch firmware headers");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "No OTA partition available");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Writing OTA to partition: %s", update_partition->label);

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }

    char ota_buffer[4096];
    size_t total_written = 0;

    while (1) {
        int data_read = esp_http_client_read(client, ota_buffer, sizeof(ota_buffer));

        if (data_read < 0) {
            ESP_LOGE(TAG, "Error while reading firmware");
            esp_ota_abort(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (data_read == 0) {
            break;
        }

        err = esp_ota_write(ota_handle, ota_buffer, data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
            esp_ota_abort(ota_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return err;
        }

        total_written += data_read;
    }

    ESP_LOGI(TAG, "Downloaded %u bytes", (unsigned)total_written);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return err;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "OTA successful, rebooting...");
    esp_restart();

    return ESP_OK;
}

void ota_task(void *pvParameters)
{
    (void)pvParameters;

    char response[1024];

    while (1) {
        memset(response, 0, sizeof(response));

        if (http_get_text(UPDATE_URL, response, sizeof(response)) == ESP_OK) {
            ESP_LOGI(TAG, "Update info: %s", response);

            cJSON *root = cJSON_Parse(response);
            if (root != NULL) {
                cJSON *target_version = cJSON_GetObjectItem(root, "target_version");
                cJSON *firmware_url = cJSON_GetObjectItem(root, "firmware_url");

                if (cJSON_IsString(target_version) &&
                    cJSON_IsString(firmware_url) &&
                    target_version->valuestring != NULL &&
                    firmware_url->valuestring != NULL) {

                    const esp_app_desc_t *app_desc = esp_app_get_description();
                    const char *current_version = app_desc->version;

                    ESP_LOGI(TAG, "Current version: %s", current_version);
                    ESP_LOGI(TAG, "Target version : %s", target_version->valuestring);
                    ESP_LOGI(TAG, "Firmware URL   : %s", firmware_url->valuestring);

                    if (compare_versions(target_version->valuestring, current_version) > 0) {
                        char firmware_url_copy[256];
                        strncpy(firmware_url_copy, firmware_url->valuestring, sizeof(firmware_url_copy) - 1);
                        firmware_url_copy[sizeof(firmware_url_copy) - 1] = '\0';

                        cJSON_Delete(root);

                        ESP_LOGI(TAG, "New version found, starting OTA...");
                        perform_http_ota(firmware_url_copy);
                    } else {
                        ESP_LOGI(TAG, "Already up to date");
                        cJSON_Delete(root);
                    }
                } else {
                    ESP_LOGW(TAG, "Invalid JSON fields");
                    cJSON_Delete(root);
                }
            } else {
                ESP_LOGW(TAG, "Failed to parse JSON");
            }
        } else {
            ESP_LOGW(TAG, "Failed to fetch update info");
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}