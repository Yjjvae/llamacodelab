add_executable(llcl-server main.cpp)
target_include_directories(llcl-server PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
  llcl-server
  PRIVATE CLI11::CLI11 llcl_http llcl_application llcl_ingestion llcl_retrieval llcl_llama llcl_support
)
llcl_set_project_warnings(llcl-server)
llcl_enable_sanitizers(llcl-server)
