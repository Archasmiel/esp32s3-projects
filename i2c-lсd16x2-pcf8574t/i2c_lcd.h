#ifndef I2C_LCD_H
#define I2C_LCD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lcd_init(uint8_t lcd_cols, uint8_t lcd_rows);
void lcd_clear(void);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_send_string(const char *str);

#ifdef __cplusplus
}
#endif

#endif