#include "i2c_lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    lcd_init(16, 2);

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("HELLO");
    lcd_set_cursor(0, 1);
    lcd_send_string("ESP32-S3");
    vTaskDelay(pdMS_TO_TICKS(5000));

    lcd_clear();
    lcd_set_cursor(0, 0);
    lcd_send_string("SECOND");
    lcd_set_cursor(0, 1);
    lcd_send_string("MESSAGE");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}