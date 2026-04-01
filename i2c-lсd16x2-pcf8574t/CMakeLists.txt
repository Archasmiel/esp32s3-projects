idf_component_register(SRCS "blink_test.c" "i2c_lcd.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c)
