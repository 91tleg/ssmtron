set(FMT_SOURCES fmt.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(fmt)
  zephyr_library_sources(${FMT_SOURCES})
  # See firmware/hal/nrf52/CMakeLists.txt: add_subdirectory from the app
  # runs in Zephyr CMake application mode, so this library misses the
  # automatic dependency on generated headers a real Zephyr module gets.
  if(TARGET zephyr_generated_headers)
    add_dependencies(fmt zephyr_generated_headers)
  endif()
else()
  add_library(fmt STATIC ${FMT_SOURCES})
endif()

target_include_directories(fmt PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(fmt PUBLIC ecu)
