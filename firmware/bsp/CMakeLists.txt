# Caller selects one board: set(BSP_BOARD <dir under bsp/>) or -DBSP_BOARD=...
# The board's devicetree overlay (bsp/<board>/board.overlay) has to be named
# before find_package(Zephyr), so the app sets DTC_OVERLAY_FILE itself.
if(NOT BSP_BOARD OR NOT EXISTS ${CMAKE_CURRENT_LIST_DIR}/${BSP_BOARD}/bsp.h)
  message(FATAL_ERROR "BSP_BOARD='${BSP_BOARD}' has no bsp/${BSP_BOARD}/bsp.h")
endif()

add_library(bsp INTERFACE)
target_include_directories(bsp INTERFACE ${CMAKE_CURRENT_LIST_DIR}/${BSP_BOARD})
target_link_libraries(bsp INTERFACE hal)
