set(GC9A01_SOURCES gc9a01.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(gc9a01)
  zephyr_library_sources(${GC9A01_SOURCES})
  # Every pixel of every frame passes through its byte-swapping loop, so it
  # is built for speed and not for size.
  zephyr_library_compile_options(-O2)
  # See firmware/hal/nrf52/CMakeLists.txt: add_subdirectory from the app
  # runs in Zephyr CMake application mode, so this library misses the
  # automatic dependency on generated headers a real Zephyr module gets.
  if(TARGET zephyr_generated_headers)
    add_dependencies(gc9a01 zephyr_generated_headers)
  endif()
else()
  add_library(gc9a01 STATIC ${GC9A01_SOURCES})
endif()

target_include_directories(gc9a01 PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(gc9a01 PUBLIC display hal)
