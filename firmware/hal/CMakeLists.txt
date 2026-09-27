# Caller selects one chip backend: set(HAL_CHIP <dir under hal/>) or -DHAL_CHIP=...
if(NOT HAL_CHIP OR NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${HAL_CHIP}/CMakeLists.txt)
  message(FATAL_ERROR "HAL_CHIP='${HAL_CHIP}' has no hal/${HAL_CHIP}/CMakeLists.txt")
endif()

add_subdirectory(${HAL_CHIP})

# Layers above the HAL link `hal`, never the chip's own target.
add_library(hal INTERFACE)
target_link_libraries(hal INTERFACE hal_${HAL_CHIP})
