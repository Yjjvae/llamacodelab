add_executable(llcl-cli main.cpp)

target_include_directories(llcl-cli PRIVATE "${PROJECT_SOURCE_DIR}/src")

target_link_libraries(
  llcl-cli
  PRIVATE
    CLI11::CLI11
    llcl_llama
    llcl_support
)

llcl_set_project_warnings(llcl-cli)
llcl_enable_sanitizers(llcl-cli)
