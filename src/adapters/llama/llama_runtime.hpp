#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace llcl::llama_adapter {

struct BackendDevice {
  std::string name;
  std::string description;
  std::size_t free_memory_bytes{};
  std::size_t total_memory_bytes{};
  bool is_gpu{};
};

class LlamaRuntime {
public:
  LlamaRuntime();
  ~LlamaRuntime();

  LlamaRuntime(const LlamaRuntime&) = delete;
  LlamaRuntime& operator=(const LlamaRuntime&) = delete;
  LlamaRuntime(LlamaRuntime&&) = delete;
  LlamaRuntime& operator=(LlamaRuntime&&) = delete;

  [[nodiscard]] std::vector<BackendDevice> devices() const;
  [[nodiscard]] bool supports_gpu_offload() const noexcept;
};

} // namespace llcl::llama_adapter
