# Software in the loop: the real gauge, input, ECU glue and SSM1 session
# against a simulated ECU on a fake serial line, in simulated time. Each test
# is its own program, and each includes harness.h for the fakes.
set(SIL_APP_SOURCES
  ../../app/ecu_io.c ../../app/gauge.c ../../app/gauge_flags.c
  ../../app/gauge_info.c ../../app/gauge_input.c ../../app/gauge_number.c
  ../../app/gauge_update.c ../../app/gauge_settings.c ../../app/gauge_sweep.c)

foreach(t boot pages clear)
  add_executable(test_sil_${t} test_sil_${t}.c ${SIL_APP_SOURCES})
  target_include_directories(test_sil_${t} PRIVATE ../../app ../../hal
                             ../../lib/display ../../lib/ui)
  target_link_libraries(test_sil_${t} PRIVATE ssm1 fmt textfit)
endforeach()

foreach(t boot pages clear)
  add_test(NAME sil_${t} COMMAND test_sil_${t})
  set_tests_properties(sil_${t} PROPERTIES LABELS sil)
endforeach()
