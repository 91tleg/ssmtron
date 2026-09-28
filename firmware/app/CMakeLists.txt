# west build -b nrf52840dk/nrf52840 firmware/app -d firmware/build
# (sysbuild: builds MCUboot too, see sysbuild.conf)
cmake_minimum_required(VERSION 3.20.0)

if(NOT HAL_CHIP)
  set(HAL_CHIP nrf52)
endif()
if(NOT BSP_BOARD)
  set(BSP_BOARD nrf52840dk)
endif()

# Must be named before find_package(Zephyr).
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../bsp/${BSP_BOARD}/board.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ssmtron)

add_subdirectory(../hal hal)
add_subdirectory(../bsp bsp)
add_subdirectory(../lib/ecu ecu)
add_subdirectory(../lib/ssm1 ssm1)
add_subdirectory(../lib/fmt fmt)
add_subdirectory(../lib/display display)
add_subdirectory(../lib/display/gc9a01 gc9a01)
add_subdirectory(../lib/ui ui)

target_sources(app PRIVATE main.c ecu_io.c ble_log.c fw_info.c gauge.c)
target_link_libraries(app PRIVATE bsp fmt gc9a01 hal ssm1 ui)

# See firmware/hil/: this file is absent from every normal build, not just
# inert, so a production image cannot contain the hang hook by accident.
if(CONFIG_APP_HIL_TEST)
  target_sources(app PRIVATE hil_test.c)
endif()
