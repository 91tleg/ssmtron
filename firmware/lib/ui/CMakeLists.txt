set(UI_SOURCES text.c font.c font_b612.c dial.c dial_motion.c)

if(COMMAND zephyr_library_named)
  zephyr_library_named(ui)
  zephyr_library_sources(${UI_SOURCES})
  # Ensure Zephyr headers exist before compiling this library.
  if(TARGET zephyr_generated_headers)
    add_dependencies(ui zephyr_generated_headers)
  endif()
else()
  add_library(ui STATIC ${UI_SOURCES})
endif()

target_include_directories(ui PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ui PUBLIC display textfit)
