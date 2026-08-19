add_library(llcl_domain STATIC domain/similarity.cpp)
target_include_directories(
  llcl_domain
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)

add_library(llcl_support STATIC support/config.cpp support/hash.cpp support/logging.cpp)
target_include_directories(
  llcl_support
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_link_libraries(
  llcl_support
  PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog
)
llcl_set_project_warnings(llcl_support)
llcl_enable_sanitizers(llcl_support)

add_library(
  llcl_ingestion
  STATIC
  adapters/filesystem/file_scanner.cpp
  adapters/filesystem/text_chunker.cpp
)
target_include_directories(
  llcl_ingestion
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
)
target_link_libraries(llcl_ingestion PUBLIC llcl_domain llcl_support)
llcl_set_project_warnings(llcl_ingestion)
llcl_enable_sanitizers(llcl_ingestion)

add_library(
  llcl_retrieval
  STATIC
  adapters/vector/brute_force_index.cpp
)
target_include_directories(
  llcl_retrieval
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  PRIVATE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>"
)
target_link_libraries(llcl_retrieval PUBLIC llcl_domain)
llcl_set_project_warnings(llcl_retrieval)
llcl_enable_sanitizers(llcl_retrieval)

add_library(llcl_application STATIC application/chat_session.cpp application/prompt_builder.cpp)
target_include_directories(
  llcl_application
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
)
target_link_libraries(llcl_application PUBLIC llcl_domain)
llcl_set_project_warnings(llcl_application)
llcl_enable_sanitizers(llcl_application)

add_library(
  llcl_llama
  STATIC
  adapters/llama/llama_runtime.cpp
  adapters/llama/llama_chat_template.cpp
  adapters/llama/llama_generator.cpp
  adapters/llama/llama_embedder.cpp
)
target_include_directories(
  llcl_llama
  PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  PRIVATE "${PROJECT_SOURCE_DIR}/src"
)
target_include_directories(
  llcl_llama
  SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/llama.cpp/include"
    "${PROJECT_SOURCE_DIR}/third_party/llama.cpp/ggml/include"
)
target_link_libraries(llcl_llama PUBLIC llcl_domain PRIVATE llama ggml llcl_support)
llcl_set_project_warnings(llcl_llama)
llcl_enable_sanitizers(llcl_llama)
