zephyr_library_named(hal_nrf52)
zephyr_library_sources(hal_dfu.c hal_gpio.c hal_time.c hal_uart.c)
# Bluetooth is opt-in: the app enables CONFIG_BT_NUS in its prj.conf.
zephyr_library_sources_ifdef(CONFIG_BT_NUS hal_ble.c)
target_include_directories(hal_nrf52 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
