#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/domain/generation.hpp"
#include "llamacodelab/support/config.hpp"
#include "llamacodelab/support/logging.hpp"

#include <CLI/CLI.hpp>

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t interrupted = 0;

extern "C" void handle_interrupt(int /* signal */) { interrupted = 1; }

[[nodiscard]] double to_mib(std::size_t bytes) {
  constexpr double bytes_per_mib = 1024.0 * 1024.0;
  return static_cast<double>(bytes) / bytes_per_mib;
}

void print_devices(const llcl::llama_adapter::LlamaRuntime& runtime) {
  const auto devices = runtime.devices();
  std::cout << "gpu_offload_supported=" << std::boolalpha << runtime.supports_gpu_offload()
            << '\n';
  if (devices.empty()) {
    std::cout << "No backend devices registered\n";
    return;
  }

  for (const auto& device : devices) {
    std::cout << "name=" << device.name << " type=" << (device.is_gpu ? "GPU" : "CPU")
              << " description=\"" << device.description << "\"";
    if (device.total_memory_bytes > 0) {
      std::cout << std::fixed << std::setprecision(0)
                << " memory_mib=" << to_mib(device.total_memory_bytes)
                << " free_mib=" << to_mib(device.free_memory_bytes);
    }
    std::cout << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"Local C++ codebase assistant powered by llama.cpp", "llcl-cli"};
  app.require_subcommand(1);

  auto* devices_command = app.add_subcommand("devices", "List llama.cpp backend devices");

  std::string config_path = "configs/default.json";
  std::string prompt;
  std::int32_t max_tokens = 512;
  float temperature = 0.2F;
  float top_p = 0.9F;
  std::uint32_t seed = 42;

  auto* generate_command = app.add_subcommand("generate", "Generate one streamed response");
  generate_command->add_option("--config", config_path, "JSON configuration file")
      ->check(CLI::ExistingFile);
  generate_command->add_option("--prompt", prompt, "Prompt text")->required();
  generate_command->add_option("--max-tokens", max_tokens, "Maximum generated tokens")
      ->check(CLI::NonNegativeNumber);
  generate_command->add_option("--temperature", temperature, "Sampling temperature")
      ->check(CLI::NonNegativeNumber);
  generate_command->add_option("--top-p", top_p, "Nucleus sampling probability")
      ->check(CLI::Range(0.000001, 1.0));
  generate_command->add_option("--seed", seed, "Sampler seed");

  CLI11_PARSE(app, argc, argv);

  try {
    if (*devices_command) {
      llcl::llama_adapter::LlamaRuntime runtime;
      print_devices(runtime);
      return 0;
    }

    const auto config = llcl::load_config(config_path);
    llcl::configure_logging(config.log_level);
    llcl::llama_adapter::LlamaRuntime runtime;
    llcl::llama_adapter::LlamaGenerator generator(runtime, config.generation_model);

    std::signal(SIGINT, handle_interrupt);
    interrupted = 0;
    std::stop_source generation_stop;
    std::jthread interrupt_bridge([&generation_stop](std::stop_token bridge_stop) {
      while (!bridge_stop.stop_requested() && interrupted == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      if (interrupted != 0) {
        generation_stop.request_stop();
      }
    });

    const llcl::GenerationOptions options{
        .max_tokens = max_tokens,
        .temperature = temperature,
        .top_p = top_p,
        .seed = seed,
    };
    const auto stats = generator.generate(
        prompt,
        options,
        [](std::string_view token) {
          std::cout << token;
          std::cout.flush();
        },
        generation_stop.get_token());
    interrupt_bridge.request_stop();

    std::cout << '\n';
    std::cerr << std::fixed << std::setprecision(2)
              << "prompt_tokens=" << stats.prompt_tokens << '\n'
              << "generated_tokens=" << stats.generated_tokens << '\n'
              << "ttft_ms=" << stats.time_to_first_token.count() << '\n'
              << "prompt_tps=" << stats.prompt_tokens_per_second << '\n'
              << "decode_tps=" << stats.decode_tokens_per_second << '\n'
              << "model_load_ms=" << generator.model_load_time().count() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
