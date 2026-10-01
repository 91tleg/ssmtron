# Software in the loop: the real gauge, input, ECU glue and SSM1 session
# against a simulated ECU on a fake serial line, in simulated time. Each test
# is its own program, and each includes harness.h for the fakes.
set(SIL_APP_SOURCES
  ../../app/ecu_io.c ../../app/gauge.c ../../app/gauge_flags.c
  ../../app/gauge_info.c ../../app/gauge_input.c ../../app/gauge_number.c
  ../../app/gauge_update.c ../../app/gauge_settings.c ../../app/gauge_sweep.c)

foreach(t boot pages clear update persist)
  add_executable(test_sil_${t} test_sil_${t}.c ${SIL_APP_SOURCES})
  target_include_directories(test_sil_${t} PRIVATE ../../app ../../hal
                             ../../lib/display ../../lib/ui)
  target_link_libraries(test_sil_${t} PRIVATE ssm1 fmt textfit)
endforeach()

foreach(t boot pages clear update)
  add_test(NAME sil_${t} COMMAND test_sil_${t})
  set_tests_properties(sil_${t} PROPERTIES LABELS sil)
endforeach()

# The restart needs two starts of the program. The save is setup for the
# restore, so running the restore alone runs it first, and the restore always
# reads a file the save just wrote.
add_test(NAME sil_persist_save COMMAND test_sil_persist save)
add_test(NAME sil_persist_restore COMMAND test_sil_persist restore)
set_tests_properties(sil_persist_save PROPERTIES FIXTURES_SETUP sil_flash
                     LABELS sil)
set_tests_properties(sil_persist_restore PROPERTIES FIXTURES_REQUIRED sil_flash
                     LABELS sil)
