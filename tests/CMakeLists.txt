include(GoogleTest)

add_executable(
  llcl_unit_tests
  unit/config_test.cpp
  unit/fake_generator_test.cpp
)
target_include_directories(llcl_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(llcl_unit_tests PRIVATE GTest::gtest_main llcl_support llcl_domain)
llcl_set_project_warnings(llcl_unit_tests)
llcl_enable_sanitizers(llcl_unit_tests)
gtest_discover_tests(llcl_unit_tests)

if(LLCL_BUILD_MODEL_TESTS)
  add_executable(llcl_model_tests integration/llama_smoke_test.cpp)
  target_include_directories(
    llcl_model_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
  )
  target_link_libraries(llcl_model_tests PRIVATE GTest::gtest_main llcl_llama)
  llcl_set_project_warnings(llcl_model_tests)
  llcl_enable_sanitizers(llcl_model_tests)
  gtest_discover_tests(llcl_model_tests PROPERTIES LABELS model)
endif()
