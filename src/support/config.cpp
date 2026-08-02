#include "llamacodelab/support/config.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace llcl {
namespace {

using Json = nlohmann::json;

[[nodiscard]] std::size_t read_size(
    const Json& object,
    std::string_view key,
    std::size_t fallback) {
  const auto iterator = object.find(key);
  if (iterator == object.end()) {
    return fallback;
  }
  if (!iterator->is_number_unsigned() && !iterator->is_number_integer()) {
    throw std::invalid_argument(std::string(key) + " must be an integer");
  }

  const auto value = iterator->get<std::int64_t>();
  if (value < 0) {
    throw std::invalid_argument(std::string(key) + " must not be negative");
  }
  if (static_cast<std::uint64_t>(value) > std::numeric_limits<std::size_t>::max()) {
    throw std::invalid_argument(std::string(key) + " is too large");
  }
  return static_cast<std::size_t>(value);
}

[[nodiscard]] ModelConfig read_model(
    const Json& root,
    std::string_view key,
    const ModelConfig& fallback) {
  const auto iterator = root.find(key);
  if (iterator == root.end()) {
    return fallback;
  }
  if (!iterator->is_object()) {
    throw std::invalid_argument(std::string(key) + " must be an object");
  }

  ModelConfig result = fallback;
  if (const auto path = iterator->find("path"); path != iterator->end()) {
    if (!path->is_string()) {
      throw std::invalid_argument(std::string(key) + ".path must be a string");
    }
    result.path = path->get<std::string>();
  }
  result.context_size = read_size(*iterator, "context_size", fallback.context_size);
  result.batch_size = read_size(*iterator, "batch_size", fallback.batch_size);
  result.gpu_layers = iterator->value("gpu_layers", fallback.gpu_layers);
  result.flash_attention = iterator->value("flash_attention", fallback.flash_attention);
  return result;
}

[[nodiscard]] IndexConfig read_index(const Json& root) {
  IndexConfig result;
  const auto iterator = root.find("index");
  if (iterator == root.end()) {
    return result;
  }
  if (!iterator->is_object()) {
    throw std::invalid_argument("index must be an object");
  }

  if (const auto data_dir = iterator->find("data_dir"); data_dir != iterator->end()) {
    if (!data_dir->is_string()) {
      throw std::invalid_argument("index.data_dir must be a string");
    }
    result.data_dir = data_dir->get<std::string>();
  }
  result.chunk_lines = read_size(*iterator, "chunk_lines", result.chunk_lines);
  result.overlap_lines = read_size(*iterator, "overlap_lines", result.overlap_lines);
  result.top_k = read_size(*iterator, "top_k", result.top_k);
  return result;
}

void validate_model(const ModelConfig& config, std::string_view name) {
  if (config.path.empty()) {
    throw std::invalid_argument(std::string(name) + ".path must not be empty");
  }
  if (config.context_size < 128 || config.context_size > 1'048'576) {
    throw std::invalid_argument(
        std::string(name) + ".context_size must be in [128, 1048576]");
  }
  if (config.batch_size == 0 || config.batch_size > config.context_size) {
    throw std::invalid_argument(
        std::string(name) + ".batch_size must be in [1, context_size]");
  }
  if (config.gpu_layers < -1 || config.gpu_layers > 999) {
    throw std::invalid_argument(std::string(name) + ".gpu_layers must be in [-1, 999]");
  }
}

}  // namespace

AppConfig load_config(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("cannot open config file: " + path.string());
  }

  Json root;
  try {
    input >> root;
  } catch (const Json::parse_error& error) {
    throw std::runtime_error(
        "failed to parse config '" + path.string() + "' at byte " +
        std::to_string(error.byte) + ": " + error.what());
  }

  if (!root.is_object()) {
    throw std::invalid_argument("config root must be a JSON object: " + path.string());
  }

  AppConfig result;
  result.generation_model = read_model(root, "generation_model", {});
  result.embedding_model = read_model(root, "embedding_model", result.generation_model);
  result.index = read_index(root);
  result.log_level = root.value("log_level", result.log_level);
  validate_config(result);
  return result;
}

void validate_config(const AppConfig& config) {
  validate_model(config.generation_model, "generation_model");
  validate_model(config.embedding_model, "embedding_model");

  if (config.index.data_dir.empty()) {
    throw std::invalid_argument("index.data_dir must not be empty");
  }
  if (config.index.chunk_lines == 0) {
    throw std::invalid_argument("index.chunk_lines must be positive");
  }
  if (config.index.overlap_lines >= config.index.chunk_lines) {
    throw std::invalid_argument("index.overlap_lines must be smaller than chunk_lines");
  }
  if (config.index.top_k == 0 || config.index.top_k > 1'000) {
    throw std::invalid_argument("index.top_k must be in [1, 1000]");
  }
  if (config.log_level != "trace" && config.log_level != "debug" &&
      config.log_level != "info" && config.log_level != "warn" &&
      config.log_level != "error" && config.log_level != "critical" &&
      config.log_level != "off") {
    throw std::invalid_argument("unsupported log_level: " + config.log_level);
  }
}

}  // namespace llcl
