#include "i2c_lcd.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include <stdint.h>

// =========================
// I2C config
// =========================
#define I2C_SDA_GPIO               8
#define I2C_SCL_GPIO               9
#define I2C_PORT                   I2C_NUM_0
#define I2C_FREQ_HZ                100000

// LCD backpack I2C address
#define LCD_I2C_ADDR               0x27

// =========================
// PCF8574 -> LCD bit mapping
// P0 = RS
// P1 = RW
// P2 = EN
// P3 = Backlight
// P4 = D4
// P5 = D5
// P6 = D6
// P7 = D7
// =========================
#define En                         0x04
#define Rw                         0x02
#define Rs                         0x01

#define LCD_BACKLIGHT              0x08
#define LCD_NOBACKLIGHT            0x00

// =========================
// LCD commands
// =========================
#define LCD_CLEARDISPLAY           0x01
#define LCD_RETURNHOME             0x02
#define LCD_ENTRYMODESET           0x04
#define LCD_DISPLAYCONTROL         0x08
#define LCD_FUNCTIONSET            0x20
#define LCD_SETDDRAMADDR           0x80

#define LCD_ENTRYLEFT              0x02
#define LCD_ENTRYSHIFTDECREMENT    0x00

#define LCD_DISPLAYON              0x04
#define LCD_CURSOROFF              0x00
#define LCD_BLINKOFF               0x00

#define LCD_4BITMODE               0x00
#define LCD_2LINE                  0x08
#define LCD_1LINE                  0x00
#define LCD_5x8DOTS                0x00

static const char *TAG = "LCD";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_lcd_dev = NULL;

static uint8_t _displayfunction = 0;
static uint8_t _displaymode = 0;
static uint8_t _displaycontrol = 0;
static uint8_t _numlines = 0;
static uint8_t _backlightval = LCD_BACKLIGHT;

// -------------------------
// low-level helpers
// -------------------------
static void delay_us(uint32_t us)
{
    esp_rom_delay_us(us);
}

static esp_err_t i2c_master_init_lcd(void)
{
    if (s_i2c_bus && s_lcd_dev) {
        return ESP_OK;
    }

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_i2c_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_I2C_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    return i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_lcd_dev);
}

static void expander_write(uint8_t data)
{
    uint8_t out = data | _backlightval;

    esp_err_t err = i2c_master_transmit(s_lcd_dev, &out, 1, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(err));
    }
}

static void pulse_enable(uint8_t data)
{
    expander_write(data | En);
    delay_us(1);

    expander_write(data & ~En);
    delay_us(50);
}

static void write4bits(uint8_t value)
{
    expander_write(value);
    pulse_enable(value);
}

static void lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t highnib = value & 0xF0;
    uint8_t lownib  = (value << 4) & 0xF0;

    write4bits(highnib | mode);
    write4bits(lownib | mode);
}

static void lcd_command(uint8_t value)
{
    lcd_send(value, 0);
}

static void lcd_write(uint8_t value)
{
    lcd_send(value, Rs);
}

void lcd_clear(void)
{
    lcd_command(LCD_CLEARDISPLAY);
    delay_us(2000);
}

static void lcd_home(void)
{
    lcd_command(LCD_RETURNHOME);
    delay_us(2000);
}

static void lcd_display_on(void)
{
    _displaycontrol |= LCD_DISPLAYON;
    lcd_command(LCD_DISPLAYCONTROL | _displaycontrol);
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    int row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row > _numlines - 1) {
        row = _numlines - 1;
    }
    lcd_command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

void lcd_send_string(const char *str)
{
    while (*str) {
        lcd_write((uint8_t)*str++);
    }
}

// -------------------------
// init
// -------------------------
static void lcd_begin(uint8_t cols, uint8_t lines, uint8_t dotsize)
{
    (void)cols;
    (void)dotsize;

    if (lines > 1) {
        _displayfunction |= LCD_2LINE;
    }
    _numlines = lines;

    delay_us(50000);

    expander_write(0x00);
    delay_us(4500);

    write4bits(0x03 << 4);
    delay_us(4500);

    write4bits(0x03 << 4);
    delay_us(4500);

    write4bits(0x03 << 4);
    delay_us(150);

    write4bits(0x02 << 4);

    lcd_command(LCD_FUNCTIONSET | _displayfunction);

    _displaycontrol = LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;
    lcd_display_on();

    lcd_clear();

    _displaymode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    lcd_command(LCD_ENTRYMODESET | _displaymode);

    lcd_home();
}

void lcd_init(uint8_t lcd_cols, uint8_t lcd_rows)
{
    esp_err_t err = i2c_master_init_lcd();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C init failed: %s", esp_err_to_name(err));
        return;
    }

    _displayfunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
    _backlightval = LCD_BACKLIGHT;

    lcd_begin(lcd_cols, lcd_rows, LCD_5x8DOTS);
}