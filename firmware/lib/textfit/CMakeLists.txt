set(TEXTFIT_SOURCES textfit.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(textfit)
  zephyr_library_sources(${TEXTFIT_SOURCES})
  # See firmware/lib/fmt/CMakeLists.txt for why this dependency is added.
  if(TARGET zephyr_generated_headers)
    add_dependencies(textfit zephyr_generated_headers)
  endif()
else()
  add_library(textfit STATIC ${TEXTFIT_SOURCES})
endif()

target_include_directories(textfit PUBLIC ${CMAKE_CURRENT_LIST_DIR})
