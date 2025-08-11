idf_component_register(SRCS
        main.c wifi.c socota.c sockhelper.c util.c socirtx.c socirnec.c

        REQUIRES nvs_flash esp_event esp_netif esp_wifi esp_system app_update esp_driver_uart
        esp_driver_rmt esp_driver_gpio esp_timer

        INCLUDE_DIRS ".")
