#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace llcl {

struct ModelConfig {
  std::filesystem::path path;
  std::size_t context_size{4096};
  std::size_t batch_size{512};
  int gpu_layers{-1};
  bool flash_attention{true};
  std::optional<std::string> chat_template;
};

struct IndexConfig {
  std::filesystem::path data_dir{"var/index"};
  std::size_t chunk_lines{80};
  std::size_t overlap_lines{16};
  std::size_t top_k{8};
};

struct AppConfig {
  ModelConfig generation_model;
  ModelConfig embedding_model;
  IndexConfig index;
  std::string log_level{"info"};
};

[[nodiscard]] AppConfig load_config(const std::filesystem::path& path);
void validate_config(const AppConfig& config);

} // namespace llcl
