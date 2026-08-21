include(GoogleTest)

add_executable(
  llcl_unit_tests
  unit/config_test.cpp
  unit/fake_generator_test.cpp
  unit/prompt_builder_test.cpp
  unit/chat_session_test.cpp
  unit/file_scanner_test.cpp
  unit/text_chunker_test.cpp
  unit/similarity_test.cpp
  unit/brute_force_index_test.cpp
  unit/hnsw_index_test.cpp
  unit/context_budget_test.cpp
  unit/ask_service_test.cpp
  unit/vector_file_test.cpp
  unit/generation_queue_test.cpp
  integration/incremental_index_test.cpp
  integration/hnsw_recall_test.cpp
  integration/http_api_test.cpp
)
target_include_directories(llcl_unit_tests PRIVATE "${PROJECT_SOURCE_DIR}/tests")
target_link_libraries(
  llcl_unit_tests
  PRIVATE GTest::gtest_main llcl_http llcl_application llcl_ingestion llcl_retrieval llcl_support llcl_domain
)
llcl_set_project_warnings(llcl_unit_tests)
llcl_enable_sanitizers(llcl_unit_tests)
gtest_discover_tests(llcl_unit_tests)

if(LLCL_BUILD_MODEL_TESTS)
  add_executable(
    llcl_model_tests
    integration/llama_smoke_test.cpp
    integration/stream_cancel_test.cpp
    integration/embedding_smoke_test.cpp
  )
  target_include_directories(
    llcl_model_tests
    PRIVATE "${PROJECT_SOURCE_DIR}/src"
  )
  target_link_libraries(
    llcl_model_tests PRIVATE GTest::gtest_main llcl_llama llcl_retrieval llcl_ingestion
  )
  target_compile_definitions(llcl_model_tests PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  llcl_set_project_warnings(llcl_model_tests)
  llcl_enable_sanitizers(llcl_model_tests)
  gtest_discover_tests(llcl_model_tests PROPERTIES LABELS model)
endif()
