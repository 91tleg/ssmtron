set(SSM1_SOURCES ssm1.c ssm1_cmd.c ssm1_rx.c ssm1_params.c ssm1_romid.c ssm1_decode.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(ssm1)
  zephyr_library_sources(${SSM1_SOURCES})
  # See firmware/hal/nrf52/CMakeLists.txt: add_subdirectory from the app
  # runs in Zephyr CMake application mode, so this library misses the
  # automatic dependency on generated headers a real Zephyr module gets.
  if(TARGET zephyr_generated_headers)
    add_dependencies(ssm1 zephyr_generated_headers)
  endif()
else()
  add_library(ssm1 STATIC ${SSM1_SOURCES})
endif()

target_include_directories(ssm1 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ssm1 PUBLIC ecu)
