get_filename_component (testName ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Tests
if (ALBANY_PANZER_EXPR_EVAL)
  # These tests require to set some input fields via a math expression.
  # If PanzerExprEval is not found in Trilinos, we can't run them.
  configure_file (${CMAKE_CURRENT_SOURCE_DIR}/input_suite_A.yaml
                  ${CMAKE_CURRENT_BINARY_DIR}/input_suite_A.yaml COPYONLY)
  add_test (${testName}_suite_A   ${Albany.exe} input_suite_A.yaml)
  set_tests_properties(${testName}_suite_A PROPERTIES LABELS "LandIce;Tpetra;Forward")
endif()
