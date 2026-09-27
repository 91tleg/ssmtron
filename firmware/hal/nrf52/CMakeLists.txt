zephyr_library_named(hal_nrf52)
zephyr_library_sources(hal_dfu.c hal_gpio.c hal_nvm.c hal_spi.c hal_time.c hal_uart.c)
# Bluetooth is opt-in: the app enables CONFIG_BT_NUS in its prj.conf.
zephyr_library_sources_ifdef(CONFIG_BT_NUS hal_ble.c)
target_include_directories(hal_nrf52 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)

# add_subdirectory(../hal) from the app runs in Zephyr CMake application
# mode, not module mode, so this library is never registered in Zephyr's
# own library list and misses the automatic dependency on generated
# headers (offsets.h and friends) that a real Zephyr module gets. Without
# this, a parallel build can race ahead and compile devicetree.h-including
# sources (e.g. hal_spi.c) before those headers exist.
if(TARGET zephyr_generated_headers)
  add_dependencies(hal_nrf52 zephyr_generated_headers)
endif()
