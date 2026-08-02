#include "adapters/llama/llama_runtime.hpp"

#include <ggml-backend.h>
#include <llama.h>

#include <cstddef>

namespace llcl::llama_adapter {

LlamaRuntime::LlamaRuntime() {
  ggml_backend_load_all();
  llama_backend_init();
}

LlamaRuntime::~LlamaRuntime() { llama_backend_free(); }

std::vector<BackendDevice> LlamaRuntime::devices() const {
  std::vector<BackendDevice> result;
  result.reserve(ggml_backend_dev_count());

  for (std::size_t index = 0; index < ggml_backend_dev_count(); ++index) {
    auto* device = ggml_backend_dev_get(index);
    std::size_t free_memory = 0;
    std::size_t total_memory = 0;
    ggml_backend_dev_memory(device, &free_memory, &total_memory);
    result.push_back({
        .name = ggml_backend_dev_name(device),
        .description = ggml_backend_dev_description(device),
        .free_memory_bytes = free_memory,
        .total_memory_bytes = total_memory,
        .is_gpu = ggml_backend_dev_type(device) == GGML_BACKEND_DEVICE_TYPE_GPU,
    });
  }
  return result;
}

bool LlamaRuntime::supports_gpu_offload() const noexcept {
  return llama_supports_gpu_offload();
}

}  // namespace llcl::llama_adapter
