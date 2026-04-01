idf_component_register(SRCS "main.c" "i2c_lcd.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_driver_i2c)
