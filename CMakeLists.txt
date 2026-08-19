cmake_minimum_required(VERSION 3.28)

project(
  LlamaCodeLab
  VERSION 0.4.0
  DESCRIPTION "Local C++ codebase assistant powered by llama.cpp"
  LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# The project does not use C++ Modules. Disabling CMake's dependency scanner keeps
# compile_commands.json consumable by clang-tidy when the build compiler is GCC.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(LLCL_BUILD_TESTS "Build tests" ON)
option(LLCL_BUILD_MODEL_TESTS "Build tests that can load a real GGUF model" ON)
option(LLCL_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(LLCL_ENABLE_CUDA "Enable llama.cpp CUDA backend" OFF)
option(LLCL_ENABLE_CLANG "Enable Clang LibTooling adapter" OFF)
option(LLCL_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CompilerWarnings)
include(Dependencies)
include(Sanitizers)

if(LLCL_ENABLE_CUDA)
  set(GGML_CUDA ON CACHE BOOL "" FORCE)
endif()

set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL OFF CACHE BOOL "" FORCE)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/CMakeLists.txt")
  message(
    FATAL_ERROR
      "llama.cpp submodule is missing. Run: git submodule update --init --recursive"
  )
endif()

add_subdirectory(third_party/llama.cpp EXCLUDE_FROM_ALL)
add_subdirectory(src)
add_subdirectory(apps)

if(LLCL_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(LLCL_BUILD_BENCHMARKS)
  message(STATUS "Benchmarks start at tutorial milestone M11")
endif()
