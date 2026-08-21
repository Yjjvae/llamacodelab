add_executable(llcl_chunker_benchmark bench_chunker.cpp)
target_link_libraries(llcl_chunker_benchmark PRIVATE llcl_ingestion llcl_domain)
target_include_directories(llcl_chunker_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")

add_executable(llcl_retrieval_benchmark bench_retrieval.cpp)
target_link_libraries(llcl_retrieval_benchmark PRIVATE llcl_retrieval llcl_domain)
target_include_directories(llcl_retrieval_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")

add_executable(llcl_inference_benchmark bench_inference.cpp)
target_link_libraries(llcl_inference_benchmark PRIVATE CLI11::CLI11 llcl_llama llcl_domain)
target_include_directories(llcl_inference_benchmark PRIVATE "${PROJECT_SOURCE_DIR}/src")

foreach(target llcl_chunker_benchmark llcl_retrieval_benchmark llcl_inference_benchmark)
  llcl_set_project_warnings(${target})
  llcl_enable_sanitizers(${target})
endforeach()
