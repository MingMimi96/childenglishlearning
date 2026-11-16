idf_component_register(SRCS "camera.c" "photo.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_ppa
                       PRIV_REQUIRES esp32_p4_function_ev_board esp_video esp_mm esp_timer)
