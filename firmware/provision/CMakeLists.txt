# Standalone Zephyr app for provisioning.
#
#   west build -b nrf52840dk/nrf52840 firmware/provision -d firmware/build-provision \
#       -- -DPROVISION_PASSKEY=654321
#
# WARNING: Verify that this app and the gauge app use the same flash storage
# partition before provisioning hardware. Their partition maps are generated
# independently; a shared pm_static.yml can fix the addresses.
cmake_minimum_required(VERSION 3.20.0)

if(NOT HAL_CHIP)
  set(HAL_CHIP nrf52)
endif()
if(NOT BSP_BOARD)
  set(BSP_BOARD nrf52840dk)
endif()

# The HAL compiles all device backends, so include the board overlay.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../bsp/${BSP_BOARD}/board.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ssmtron_provision)

zephyr_get(PROVISION_PASSKEY)
if(NOT DEFINED PROVISION_PASSKEY)
  message(FATAL_ERROR "Pass -DPROVISION_PASSKEY=<6-digit code>")
endif()
target_compile_definitions(app PRIVATE PROVISION_PASSKEY=${PROVISION_PASSKEY})

add_subdirectory(../hal hal)

target_sources(app PRIVATE main.c)
target_link_libraries(app PRIVATE hal)
