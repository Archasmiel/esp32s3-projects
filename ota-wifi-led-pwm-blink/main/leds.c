#include "leds.h"

#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_check.h"

#define LED_PWM_1 GPIO_NUM_1
#define LED_PWM_2 GPIO_NUM_2
#define LED_BLINK GPIO_NUM_42

#define LEDC_RESOLUTION  LEDC_TIMER_13_BIT
#define LEDC_FREQ        5000
#define LEDC_MAX_DUTY    ((1 << 13) - 1)

static const char *TAG = "leds";

static void timer_setup(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = LEDC_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
}

static void pwm1_setup(void)
{
    ledc_channel_config_t pwm1_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = LED_PWM_1,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm1_channel));
}

static void pwm2_setup(void)
{
    ledc_channel_config_t pwm2_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_0,
        .gpio_num = LED_PWM_2,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&pwm2_channel));
}

static void blink_setup(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_BLINK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static void task_pwm2(void *pvParams)
{
    (void)pvParams;

    while (1) {
        ESP_ERROR_CHECK(ledc_set_fade_with_time(
            LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_MAX_DUTY, 1000
        ));
        ESP_ERROR_CHECK(ledc_fade_start(
            LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE
        ));

        ESP_ERROR_CHECK(ledc_set_fade_with_time(
            LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, 0, 1000
        ));
        ESP_ERROR_CHECK(ledc_fade_start(
            LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, LEDC_FADE_WAIT_DONE
        ));
    }
}

static void task_blink(void *pvParams)
{
    (void)pvParams;

    bool blink_state = false;

    while (1) {
        blink_state = !blink_state;
        ESP_ERROR_CHECK(gpio_set_level(LED_BLINK, blink_state));
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void leds_init(void)
{
    timer_setup();
    pwm1_setup();
    pwm2_setup();
    blink_setup();

    ESP_ERROR_CHECK(ledc_fade_func_install(0));
}

void leds_set_pwm1_quarter(void)
{
    ESP_ERROR_CHECK(ledc_set_duty(
        LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, LEDC_MAX_DUTY / 4
    ));
    ESP_ERROR_CHECK(ledc_update_duty(
        LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0
    ));
}

void leds_start_tasks(void)
{
    xTaskCreatePinnedToCore(task_pwm2, "led_pwm_fade", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(task_blink, "led_blink", 2048, NULL, 5, NULL, tskNO_AFFINITY);
}