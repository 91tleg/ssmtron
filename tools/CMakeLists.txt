cmake_minimum_required(VERSION 3.16)
project(ssmtron_ui_preview C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(preview_ui
  preview_ui.c
  ../firmware/app/gauge.c
  ../firmware/app/gauge_flags.c
  ../firmware/app/gauge_info.c
  ../firmware/app/gauge_input.c
  ../firmware/app/gauge_number.c
  ../firmware/app/gauge_settings.c
  ../firmware/app/gauge_sweep.c
  ../firmware/app/gauge_update.c
  ../firmware/lib/fmt/fmt.c
  ../firmware/lib/textfit/textfit.c
  ../firmware/lib/ui/text.c
  ../firmware/lib/ui/dial.c
  ../firmware/lib/ui/font.c
  ../firmware/lib/ui/font_b612.c
  ../firmware/lib/ui/dial_motion.c
)
target_include_directories(preview_ui PRIVATE
  ../firmware
  ../firmware/app
  ../firmware/hal
  ../firmware/lib/ecu
  ../firmware/lib/display
  ../firmware/lib/fmt
  ../firmware/lib/ui
)
target_compile_options(preview_ui PRIVATE -Wall -Wextra -Werror
                                          -Wconversion -Wshadow -pedantic
                                          -fsanitize=address,undefined)
target_link_options(preview_ui PRIVATE -fsanitize=address,undefined)
