#include "llamacodelab/support/logging.hpp"

#include <mutex>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

namespace llcl {
namespace {

std::once_flag logger_once;

void ensure_logger() {
  std::call_once(logger_once, [] {
    auto instance = spdlog::stderr_color_mt("llamacodelab");
    instance->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    instance->set_level(spdlog::level::info);
  });
}

} // namespace

void configure_logging(std::string_view level) {
  ensure_logger();
  const auto parsed_level = spdlog::level::from_str(std::string(level));
  if (parsed_level == spdlog::level::off && level != "off") {
    throw std::invalid_argument("unsupported log level: " + std::string(level));
  }
  logger()->set_level(parsed_level);
}

std::shared_ptr<spdlog::logger> logger() {
  ensure_logger();
  return spdlog::get("llamacodelab");
}

} // namespace llcl
