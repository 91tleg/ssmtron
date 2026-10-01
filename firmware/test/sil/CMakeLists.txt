# Software in the loop: the real gauge, input, ECU glue and SSM1 session
# against a simulated ECU on a fake serial line, in simulated time. Each test
# is its own program, and each includes harness.h for the fakes.
set(SIL_APP_SOURCES
  ../../app/ecu_io.c ../../app/gauge.c ../../app/gauge_flags.c
  ../../app/gauge_info.c ../../app/gauge_input.c ../../app/gauge_number.c
  ../../app/gauge_update.c ../../app/gauge_settings.c ../../app/gauge_sweep.c)

foreach(t boot pages clear update sweep persist)
  add_executable(test_sil_${t} test_sil_${t}.c ${SIL_APP_SOURCES})
  target_include_directories(test_sil_${t} PRIVATE ../../app ../../hal
                             ../../lib/display ../../lib/ui)
  target_link_libraries(test_sil_${t} PRIVATE ssm1 fmt textfit)
endforeach()

foreach(t boot pages clear update)
  add_test(NAME sil_${t} COMMAND test_sil_${t})
  set_tests_properties(sil_${t} PROPERTIES LABELS sil)
endforeach()

# The sweep's state lasts for the whole run, like a power-up, so its two cases
# are two starts.
add_test(NAME sil_sweep COMMAND test_sil_sweep)
add_test(NAME sil_sweep_theme COMMAND test_sil_sweep theme)
set_tests_properties(sil_sweep sil_sweep_theme PROPERTIES LABELS sil)

# The restart needs two starts of the program. The save is setup for the
# restore, so running the restore alone runs it first, and the restore always
# reads a file the save just wrote.
add_test(NAME sil_persist_save COMMAND test_sil_persist save)
add_test(NAME sil_persist_restore COMMAND test_sil_persist restore)
set_tests_properties(sil_persist_save PROPERTIES FIXTURES_SETUP sil_flash
                     LABELS sil)
set_tests_properties(sil_persist_restore PROPERTIES FIXTURES_REQUIRED sil_flash
                     LABELS sil)

# The real main.c, compiled under another name so the test can call it. The
# fake delay in its loop jumps out after a set time. Each mode is its own start.
set_source_files_properties(../../app/main.c PROPERTIES COMPILE_DEFINITIONS
                            main=app_main)
add_executable(test_sil_main test_sil_main.c ../../app/main.c ${SIL_APP_SOURCES})
target_include_directories(test_sil_main PRIVATE ../../app ../../hal
                           ../../lib/display ../../lib/ui
                           ../../bsp/nrf52840dk)
target_link_libraries(test_sil_main PRIVATE ssm1 fmt textfit)
add_test(NAME sil_main COMMAND test_sil_main)
add_test(NAME sil_main_nodisplay COMMAND test_sil_main nodisplay)
add_test(NAME sil_main_nouart COMMAND test_sil_main nouart)
add_test(NAME sil_main_retry COMMAND test_sil_main retry)
add_test(NAME sil_main_nobootloader COMMAND test_sil_main nobootloader)
set_tests_properties(sil_main sil_main_nodisplay sil_main_nouart sil_main_retry
                     sil_main_nobootloader PROPERTIES LABELS sil)
