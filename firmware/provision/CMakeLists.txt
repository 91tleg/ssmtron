# A separate, tiny Zephyr application -- not a target of firmware/app's
# CMakeLists.txt -- so that provisioning a unit never means building or
# flashing anything from the real gauge firmware.
#
#   west build -b nrf52840dk/nrf52840 firmware/provision -d firmware/build-provision \
#       -- -DPROVISION_PASSKEY=654321
#
# WARNING: this build has no sysbuild/MCUboot (see firmware/app/sysbuild.conf
# for that), so nRF Connect SDK's partition manager computes this image's
# own "storage_partition" independently of the real app's build. Nothing
# here pins that address, so there is currently no guarantee the two builds
# agree on where hal_nvm's data lives in flash -- verify (or add a shared
# pm_static.yml fixing storage_partition, mcuboot, app and scratch to the
# same addresses in both builds) before trusting this on real hardware.
cmake_minimum_required(VERSION 3.20.0)

if(NOT HAL_CHIP)
  set(HAL_CHIP nrf52)
endif()
if(NOT BSP_BOARD)
  set(BSP_BOARD nrf52840dk)
endif()

# hal_nrf52 compiles hal_gpio.c/hal_uart.c/hal_spi.c unconditionally
# alongside hal_nvm.c (see firmware/hal/nrf52/CMakeLists.txt), so this
# build needs the same board devicetree properties the real app does,
# even though provisioning only calls hal_nvm.
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/../bsp/${BSP_BOARD}/board.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ssmtron_provision)

if(NOT DEFINED PROVISION_PASSKEY)
  message(FATAL_ERROR "Pass -DPROVISION_PASSKEY=<6-digit code>")
endif()
target_compile_definitions(app PRIVATE PROVISION_PASSKEY=${PROVISION_PASSKEY})

add_subdirectory(../hal hal)

target_sources(app PRIVATE main.c)
target_link_libraries(app PRIVATE hal)
