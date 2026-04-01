idf_component_register(SRCS 
                        "main.c"
                        "wifi.c"
                        "ota.c"
                        "leds.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        nvs_flash
                        esp_event
                        esp_wifi
                        esp_netif
                        esp_http_client
                        app_update
                        esp_driver_ledc 
                        esp_driver_gpio
                        espressif__cjson)
