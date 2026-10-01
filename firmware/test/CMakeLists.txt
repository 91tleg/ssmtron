# Host-side unit tests (no Zephyr, no HAL):
#   cmake -S firmware/test -B firmware/build-test && cmake --build firmware/build-test && ctest --test-dir firmware/build-test --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ssmtron_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -Wconversion -Wshadow -pedantic
                    -fsanitize=address,undefined -fno-sanitize-recover=undefined -g)
add_link_options(-fsanitize=address,undefined)

# Headers are included by their path from firmware/, for example
# "lib/ui/text.h". The per-directory include paths below still serve the
# files that have not moved to that form yet.
include_directories(${CMAKE_CURRENT_LIST_DIR}/..)

add_subdirectory(../lib/ecu ecu)
add_subdirectory(../lib/ssm1 ssm1)
add_subdirectory(../lib/fmt fmt)
add_subdirectory(../lib/textfit textfit)

enable_testing()
foreach(t ssm1_cmd ssm1_rx ssm1_romid ssm1_decode ssm1_params ssm1)
  add_executable(test_${t} test_${t}.c)
  target_link_libraries(test_${t} PRIVATE ssm1)
  add_test(NAME ${t} COMMAND test_${t})
endforeach()

# App glue against fake HAL/ECU (the fakes live in the test file).
add_executable(test_ecu_io test_ecu_io.c ../app/ecu_io.c)
target_include_directories(test_ecu_io PRIVATE ../app ../hal ../lib/ecu)
add_test(NAME ecu_io COMMAND test_ecu_io)

add_executable(test_fmt test_fmt.c)
target_link_libraries(test_fmt PRIVATE fmt)
add_test(NAME fmt COMMAND test_fmt)

add_executable(test_textfit test_textfit.c)
target_link_libraries(test_textfit PRIVATE textfit)
add_test(NAME textfit COMMAND test_textfit)

add_executable(test_ble_log test_ble_log.c ../app/ble_log.c)
target_include_directories(test_ble_log PRIVATE ../app ../hal)
target_link_libraries(test_ble_log PRIVATE fmt)
add_test(NAME ble_log COMMAND test_ble_log)

add_executable(test_gauge test_gauge.c ../app/gauge.c ../app/gauge_flags.c ../app/gauge_info.c ../app/gauge_input.c ../app/gauge_number.c ../app/gauge_update.c ../app/gauge_settings.c ../app/gauge_sweep.c)
target_include_directories(test_gauge PRIVATE ../app ../hal ../lib/display
                                               ../lib/ui)
target_link_libraries(test_gauge PRIVATE fmt textfit)
add_test(NAME gauge COMMAND test_gauge)

add_executable(test_gauge_watch_log test_gauge_watch_log.c ../app/gauge.c ../app/gauge_flags.c ../app/gauge_info.c ../app/gauge_input.c ../app/gauge_number.c ../app/gauge_update.c ../app/gauge_settings.c ../app/gauge_sweep.c)
target_include_directories(test_gauge_watch_log
                            PRIVATE ../app ../hal ../lib/display ../lib/ui)
target_link_libraries(test_gauge_watch_log PRIVATE fmt textfit)
add_test(NAME gauge_watch_log COMMAND test_gauge_watch_log)

add_executable(test_gauge_dial test_gauge_dial.c ../app/gauge.c ../app/gauge_flags.c ../app/gauge_info.c ../app/gauge_input.c ../app/gauge_number.c ../app/gauge_update.c ../app/gauge_settings.c ../app/gauge_sweep.c)
target_include_directories(test_gauge_dial
                            PRIVATE ../app ../hal ../lib/display ../lib/ui)
target_link_libraries(test_gauge_dial PRIVATE fmt textfit)
add_test(NAME gauge_dial COMMAND test_gauge_dial)

add_executable(test_gauge_flags test_gauge_flags.c ../app/gauge.c ../app/gauge_flags.c ../app/gauge_info.c ../app/gauge_input.c ../app/gauge_number.c ../app/gauge_update.c ../app/gauge_settings.c ../app/gauge_sweep.c)
target_include_directories(test_gauge_flags
                            PRIVATE ../app ../hal ../lib/display ../lib/ui)
target_link_libraries(test_gauge_flags PRIVATE fmt textfit)
add_test(NAME gauge_flags COMMAND test_gauge_flags)

add_executable(test_gauge_input test_gauge_input.c ../app/gauge_input.c)
target_include_directories(test_gauge_input PRIVATE ../app ../hal)
add_test(NAME gauge_input COMMAND test_gauge_input)

add_executable(test_gauge_sweep test_gauge_sweep.c ../app/gauge_sweep.c)
target_include_directories(test_gauge_sweep PRIVATE ../app ../hal)
add_test(NAME gauge_sweep COMMAND test_gauge_sweep)
add_test(NAME gauge_sweep_cancel COMMAND test_gauge_sweep cancel)

add_executable(test_gauge_settings test_gauge_settings.c
                                   ../app/gauge_settings.c)
target_include_directories(test_gauge_settings PRIVATE ../app ../hal)
add_test(NAME gauge_settings COMMAND test_gauge_settings)

# Includes dial.c itself, to reach its static full-frame draw.
add_executable(test_dial_region test_dial_region.c ../lib/ui/font.c
               ../lib/ui/dial_motion.c)
target_include_directories(test_dial_region PRIVATE ../lib/ui ../lib/display
                                                     ../hal)
target_link_libraries(test_dial_region PRIVATE textfit)
add_test(NAME dial_region COMMAND test_dial_region)

# The same code compiled for a 360 x 360 panel, to keep it free of hard-coded
# 240 px assumptions. This checks correctness only, not how it looks.
add_executable(test_dial_region_360 test_dial_region.c ../lib/ui/font.c
               ../lib/ui/dial_motion.c)
target_include_directories(test_dial_region_360 PRIVATE ../lib/ui
                           ../lib/display ../hal)
target_link_libraries(test_dial_region_360 PRIVATE textfit)
target_compile_definitions(test_dial_region_360 PRIVATE DISPLAY_WIDTH=360u
                                                        DISPLAY_HEIGHT=360u)
add_test(NAME dial_region_360 COMMAND test_dial_region_360)

add_executable(test_gauge_flags_360
  test_gauge_flags.c ../app/gauge.c ../app/gauge_flags.c ../app/gauge_info.c ../app/gauge_input.c ../app/gauge_number.c ../app/gauge_update.c
  ../app/gauge_settings.c ../app/gauge_sweep.c)
target_include_directories(test_gauge_flags_360
                            PRIVATE ../app ../hal ../lib/display ../lib/ui)
target_link_libraries(test_gauge_flags_360 PRIVATE fmt textfit)
target_compile_definitions(test_gauge_flags_360 PRIVATE DISPLAY_WIDTH=360u
                                                        DISPLAY_HEIGHT=360u)
add_test(NAME gauge_flags_360 COMMAND test_gauge_flags_360)

add_executable(test_gauge_info test_gauge_info.c ../app/gauge_info.c)
target_include_directories(test_gauge_info PRIVATE ../app ../hal)
add_test(NAME gauge_info COMMAND test_gauge_info)

add_executable(test_gauge_info_360 test_gauge_info.c ../app/gauge_info.c)
target_include_directories(test_gauge_info_360 PRIVATE ../app ../hal)
target_compile_definitions(test_gauge_info_360 PRIVATE DISPLAY_WIDTH=360u
                                                       DISPLAY_HEIGHT=360u)
add_test(NAME gauge_info_360 COMMAND test_gauge_info_360)

add_executable(test_gauge_number test_gauge_number.c ../app/gauge_number.c)
target_include_directories(test_gauge_number PRIVATE ../app ../hal)
target_link_libraries(test_gauge_number PRIVATE fmt textfit)
add_test(NAME gauge_number COMMAND test_gauge_number)

add_executable(test_gauge_number_360 test_gauge_number.c ../app/gauge_number.c)
target_include_directories(test_gauge_number_360 PRIVATE ../app ../hal)
target_compile_definitions(test_gauge_number_360 PRIVATE DISPLAY_WIDTH=360u
                                                         DISPLAY_HEIGHT=360u)
target_link_libraries(test_gauge_number_360 PRIVATE fmt textfit)
add_test(NAME gauge_number_360 COMMAND test_gauge_number_360)

add_executable(test_gauge_update test_gauge_update.c ../app/gauge_update.c)
target_include_directories(test_gauge_update PRIVATE ../app ../hal)
add_test(NAME gauge_update COMMAND test_gauge_update)
add_test(NAME gauge_update_zeros COMMAND test_gauge_update zeros)
add_test(NAME gauge_update_failed COMMAND test_gauge_update failed)
add_test(NAME gauge_update_norandom COMMAND test_gauge_update norandom)
add_test(NAME gauge_update_exit COMMAND test_gauge_update exit)
add_test(NAME gauge_update_firstframe COMMAND test_gauge_update firstframe)
add_test(NAME gauge_update_fit COMMAND test_gauge_update fit)

add_executable(test_gauge_update_360 test_gauge_update.c ../app/gauge_update.c)
target_include_directories(test_gauge_update_360 PRIVATE ../app ../hal)
target_compile_definitions(test_gauge_update_360 PRIVATE DISPLAY_WIDTH=360u
                                                         DISPLAY_HEIGHT=360u)
add_test(NAME gauge_update_fit_360 COMMAND test_gauge_update_360 fit)

# Compile text/font sources directly with a fake display; HAL has no host
# backend.
add_executable(test_text test_text.c ../lib/ui/text.c ../lib/ui/font.c)
target_include_directories(test_text PRIVATE ../lib/ui ../lib/display ../hal)
add_test(NAME text COMMAND test_text)

add_executable(test_dial test_dial.c ../lib/ui/dial.c ../lib/ui/font.c
               ../lib/ui/dial_motion.c)
target_include_directories(test_dial PRIVATE ../lib/ui ../lib/display ../hal)
target_link_libraries(test_dial PRIVATE textfit)
add_test(NAME dial COMMAND test_dial)

# The page modules build their rows with the shared text fitting.
foreach(t gauge_info gauge_info_360 gauge_update gauge_update_360)
  target_link_libraries(test_${t} PRIVATE textfit)
endforeach()

add_executable(test_dial_motion test_dial_motion.c ../lib/ui/dial_motion.c)
add_test(NAME dial_motion COMMAND test_dial_motion)

# The display driver against fake HAL calls. Its panel size comes from
# lib/display/display.h.
add_executable(test_gc9a01 test_gc9a01.c ../lib/display/gc9a01/gc9a01.c)
target_include_directories(test_gc9a01 PRIVATE ../lib/display ../hal)
add_test(NAME gc9a01 COMMAND test_gc9a01)

add_subdirectory(sil)
