include(FetchContent)

set(FETCHCONTENT_QUIET OFF)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/archive/65ee68451d8eb2b5f3a30b410476ab83deb3289b.tar.gz
  URL_HASH SHA256=13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f
  DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/_downloads"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  spdlog
  URL https://github.com/gabime/spdlog/archive/6fa36017cfd5731d617e1a934f0e5ea9c4445b13.tar.gz
  URL_HASH SHA256=5097fb362e79a2bd7247beaf1f8377ed60e274fbe83a4b33e7b73383f0279022
  DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/_downloads"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  cli11
  URL https://github.com/CLIUtils/CLI11/archive/4160d259d961cd393fd8d67590a8c7d210207348.tar.gz
  URL_HASH SHA256=c91e8768600e61be11f7250e3cf3e71afd9d0f18f9c9e9e209a8e084ca08cd85
  DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/_downloads"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_Declare(
  cpp_httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.20.0.tar.gz
  URL_HASH SHA256=18064587e0cc6a0d5d56d619f4cbbcaba47aa5d84d86013abbd45d95c6653866
  DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/_downloads"
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# The HTTP server only serves loopback HTTP in M8.  Disable optional transport and
# compression discovery so cpp-httplib does not inherit incomplete system CMake targets.
set(HTTPLIB_USE_OPENSSL_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZLIB_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_BROTLI_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json spdlog cli11 cpp_httplib)

if(LLCL_BUILD_TESTS)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.tar.gz
    URL_HASH SHA256=745c55415660044610f7fcd3af7a6420d5de16a7dbb9ebfe2e131275676232be
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/build/_downloads"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()
