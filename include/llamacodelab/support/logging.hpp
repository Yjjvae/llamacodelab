#pragma once

#include <memory>
#include <string_view>

namespace spdlog {
class logger;
}

namespace llcl {

void configure_logging(std::string_view level);
[[nodiscard]] std::shared_ptr<spdlog::logger> logger();

} // namespace llcl
