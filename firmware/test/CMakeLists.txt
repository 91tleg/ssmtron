# Host-side unit tests (no Zephyr, no HAL):
#   cmake -S firmware/test -B firmware/build-test && cmake --build firmware/build-test && ctest --test-dir firmware/build-test --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(ssmtron_tests C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror -Wconversion -Wshadow -pedantic
                    -fsanitize=address,undefined -fno-sanitize-recover=undefined -g)
add_link_options(-fsanitize=address,undefined)

add_subdirectory(../lib/ecu ecu)
add_subdirectory(../lib/ssm1 ssm1)
add_subdirectory(../lib/fmt fmt)

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

add_executable(test_ble_log test_ble_log.c ../app/ble_log.c)
target_include_directories(test_ble_log PRIVATE ../app ../hal)
target_link_libraries(test_ble_log PRIVATE fmt)
add_test(NAME ble_log COMMAND test_ble_log)

# Compile UI sources directly with a fake display; HAL has no host backend.
add_executable(test_ui test_ui.c ../lib/ui/ui.c ../lib/ui/ui_font.c)
target_include_directories(test_ui PRIVATE ../lib/ui ../lib/display ../hal)
add_test(NAME ui COMMAND test_ui)
