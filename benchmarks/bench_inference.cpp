#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"

#include <CLI/CLI.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stop_token>
#include <string>
#include <vector>

namespace {
[[nodiscard]] double percentile(std::vector<double> values, const double quantile) {
  std::sort(values.begin(), values.end());
  const auto index = static_cast<std::size_t>(
      std::ceil(quantile * static_cast<double>(values.size() - 1U)));
  return values[index];
}

[[nodiscard]] double peak_rss_mib() {
  std::ifstream status("/proc/self/status");
  std::string key;
  std::size_t kib{};
  while (status >> key >> kib) {
    if (key == "VmHWM:") {
      return static_cast<double>(kib) / 1024.0;
    }
    status.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
  }
  return 0.0;
}
} // namespace

int main(int argc, char** argv) {
  CLI::App app{"Repeatable llama.cpp inference benchmark"};
  std::string model;
  std::string prompt{"Explain RAII in one concise C++ paragraph."};
  int gpu_layers{-1};
  std::size_t context_size{4096};
  std::size_t batch_size{512};
  int runs{10};
  int output_tokens{128};
  std::uint32_t seed{42};
  app.add_option("--model", model)->required()->check(CLI::ExistingFile);
  app.add_option("--prompt", prompt);
  app.add_option("--gpu-layers", gpu_layers);
  app.add_option("--context-size", context_size);
  app.add_option("--batch-size", batch_size);
  app.add_option("--runs", runs)->check(CLI::PositiveNumber);
  app.add_option("--output-tokens", output_tokens)->check(CLI::PositiveNumber);
  app.add_option("--seed", seed);
  CLI11_PARSE(app, argc, argv);

  llcl::llama_adapter::LlamaRuntime runtime;
  llcl::ModelConfig config;
  config.path = model;
  config.context_size = context_size;
  config.batch_size = batch_size;
  config.gpu_layers = gpu_layers;
  llcl::llama_adapter::LlamaGenerator generator(runtime, std::move(config));
  std::vector<double> ttft;
  std::vector<double> decode;
  std::vector<double> end_to_end;
  for (int run = 0; run < runs; ++run) {
    const auto started = std::chrono::steady_clock::now();
    const auto stats = generator.generate(
        prompt, {.max_tokens = output_tokens, .temperature = 0.0F, .seed = seed},
        [](std::string_view) {}, {});
    ttft.push_back(static_cast<double>(stats.time_to_first_token.count()));
    decode.push_back(stats.decode_tokens_per_second);
    end_to_end.push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count());
  }
  std::cout << "{\"benchmark\":\"inference\",\"seed\":" << seed
            << ",\"runs\":" << runs << ",\"context_size\":" << context_size
            << ",\"batch_size\":" << batch_size << ",\"gpu_layers\":" << gpu_layers
            << ",\"model_load_ms\":" << generator.model_load_time().count()
            << ",\"ttft_p50_ms\":" << percentile(ttft, 0.50)
            << ",\"ttft_p95_ms\":" << percentile(ttft, 0.95)
            << ",\"decode_tps_p50\":" << percentile(decode, 0.50)
            << ",\"decode_tps_p95\":" << percentile(decode, 0.95)
            << ",\"end_to_end_p50_ms\":" << percentile(end_to_end, 0.50)
            << ",\"end_to_end_p95_ms\":" << percentile(end_to_end, 0.95)
            << ",\"peak_rss_mib\":" << peak_rss_mib() << "}\n";
}
