idf_component_register(
    SRCS "main.c" "c6_update.c" "app_wireless.c" "wireless/legacy_serial.c" "wireless/wire.c" "wireless/radio.c" "wireless/transport.c" "app_weather.c" "weather/weather_images.c" "app_bridge.c" "services/ble_mouse.c" "services/keymouse_service.c" "services/mouse_report.c" "services/mouse_flow.c" "services/keyboard_flow.c" "app_keys.c" "services/usb_service.c" "app_xiaozhi.c" "services/xiaozhi_service.c" "services/xiaozhi_protocol.c" "services/voice_audio.c" "app_serial.c" "services/serial_service.c" "weather/weather_service.c" "weather/weather_data.c" "weather/weather_calendar.c" "weather/weather_preferences.c" "demo_ui.c" "platform.c" "app_sample.c" "app_registry.c" "app_clock.c" "app_counter.c" "services/serial_frame.c" "services/serial_history.c" "services/serial_plot.c" "demo_storage.c" "demo_audio.c"
         "demo_camera.c" "demo_network.c" "demo_io.c" "demo_usb.c"
         "demo_text.c" "demo_console.c" "fonts/board_font_cjk_16.c" "fonts/wireless_mono_40.c" "fonts/wireless_ui_20.c" "fonts/wireless_ui_28.c" "fonts/wireless_ui_36.c"
    INCLUDE_DIRS "." "services" "weather" "wireless"
    REQUIRES espressif__esp32_p4_function_ev_board esp_cam_sensor
             espressif__ip101 esp_video esp_codec_dev lvgl
             nvs_flash esp_driver_uart esp_driver_gpio esp_driver_i2c
             esp_driver_sdmmc esp_driver_usb_serial_jtag esp_psram esp_timer esp_wifi esp_netif
             esp_event esp_eth usb fatfs esp_app_format esp_http_client mbedtls espressif__cjson esp_app_format espressif__esp_websocket_client espressif__mqtt espressif__esp_audio_codec espressif__esp_tinyusb espressif__usb_host_hid bt)









