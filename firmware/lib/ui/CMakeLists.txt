set(UI_SOURCES ui.c ui_font.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(ui)
  zephyr_library_sources(${UI_SOURCES})
  # See firmware/hal/nrf52/CMakeLists.txt: add_subdirectory from the app
  # runs in Zephyr CMake application mode, so this library misses the
  # automatic dependency on generated headers a real Zephyr module gets.
  if(TARGET zephyr_generated_headers)
    add_dependencies(ui zephyr_generated_headers)
  endif()
else()
  add_library(ui STATIC ${UI_SOURCES})
endif()

target_include_directories(ui PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ui PUBLIC display)
