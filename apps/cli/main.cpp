#include "adapters/llama/llama_generator.hpp"
#include "adapters/llama/llama_runtime.hpp"
#include "llamacodelab/application/chat_session.hpp"
#include "llamacodelab/application/prompt_builder.hpp"
#include "llamacodelab/domain/chat.hpp"
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
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

volatile std::sig_atomic_t interrupted = 0;

extern "C" void handle_interrupt(int /* signal */) {
  interrupted = 1;
}

[[nodiscard]] double to_mib(std::size_t bytes) {
  constexpr double bytes_per_mib = 1024.0 * 1024.0;
  return static_cast<double>(bytes) / bytes_per_mib;
}

void print_devices(const llcl::llama_adapter::LlamaRuntime& runtime) {
  const auto devices = runtime.devices();
  std::cout << "gpu_offload_supported=" << std::boolalpha << runtime.supports_gpu_offload() << '\n';
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

[[nodiscard]] llcl::Role parse_chat_role(std::string_view value) {
  if (value == "system") {
    return llcl::Role::system;
  }
  if (value == "user") {
    return llcl::Role::user;
  }
  if (value == "assistant") {
    return llcl::Role::assistant;
  }
  throw std::invalid_argument("chat turn role must be system, user, or assistant");
}

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"Local C++ codebase assistant powered by llama.cpp", "llcl-cli"};
  app.require_subcommand(1);

  auto* devices_command = app.add_subcommand("devices", "List llama.cpp backend devices");

  std::string config_path = "configs/default.json";
  std::string prompt;
  std::int32_t max_tokens = 512;
  float temperature = 0.2F;
  std::int32_t top_k = 40;
  float top_p = 0.9F;
  float repeat_penalty = 1.1F;
  std::uint32_t seed = 42;

  auto* generate_command = app.add_subcommand("generate", "Generate one streamed response");
  generate_command->add_option("--config", config_path, "JSON configuration file")
      ->check(CLI::ExistingFile);
  generate_command->add_option("--prompt", prompt, "Prompt text")->required();
  generate_command->add_option("--max-tokens", max_tokens, "Maximum generated tokens")
      ->check(CLI::NonNegativeNumber);
  generate_command->add_option("--temperature", temperature, "Sampling temperature")
      ->check(CLI::NonNegativeNumber);
  generate_command->add_option("--top-k", top_k, "Top-k sampling (0 disables)")
      ->check(CLI::NonNegativeNumber);
  generate_command->add_option("--top-p", top_p, "Nucleus sampling probability")
      ->check(CLI::Range(0.000001, 1.0));
  generate_command->add_option("--repeat-penalty", repeat_penalty, "Repeat penalty")
      ->check(CLI::PositiveNumber);
  generate_command->add_option("--seed", seed, "Sampler seed");

  std::string chat_config_path = "configs/default.json";
  std::string system_message;
  std::vector<std::string> user_messages;
  std::vector<std::string> chat_turns;
  std::int32_t chat_max_tokens = 512;
  float chat_temperature = 0.2F;
  std::int32_t chat_top_k = 40;
  float chat_top_p = 0.9F;
  float chat_repeat_penalty = 1.1F;
  std::uint32_t chat_seed = 42;
  auto* chat_command = app.add_subcommand("chat", "Generate from a GGUF chat template");
  chat_command->add_option("--config", chat_config_path, "JSON configuration file")
      ->check(CLI::ExistingFile);
  chat_command->add_option("--system", system_message, "Optional system message");
  chat_command->add_option("--message", user_messages, "User message; repeat for history");
  chat_command->add_option("--turn", chat_turns,
                           "History turn as role:content; repeat to preserve role order");
  chat_command->add_option("--max-tokens", chat_max_tokens, "Maximum generated tokens")
      ->check(CLI::NonNegativeNumber);
  chat_command->add_option("--temperature", chat_temperature, "Sampling temperature")
      ->check(CLI::NonNegativeNumber);
  chat_command->add_option("--top-k", chat_top_k, "Top-k sampling (0 disables)")
      ->check(CLI::NonNegativeNumber);
  chat_command->add_option("--top-p", chat_top_p, "Nucleus sampling probability")
      ->check(CLI::Range(0.000001, 1.0));
  chat_command->add_option("--repeat-penalty", chat_repeat_penalty, "Repeat penalty")
      ->check(CLI::PositiveNumber);
  chat_command->add_option("--seed", chat_seed, "Sampler seed");

  CLI11_PARSE(app, argc, argv);

  try {
    if (*devices_command) {
      llcl::llama_adapter::LlamaRuntime runtime;
      print_devices(runtime);
      return 0;
    }

    const auto active_config_path = *chat_command ? chat_config_path : config_path;
    const auto config = llcl::load_config(active_config_path);
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

    llcl::GenerationOptions options;
    std::string generation_prompt;
    std::optional<llcl::ChatSession> chat_session;
    if (*chat_command) {
      chat_session.emplace();
      auto& session = *chat_session;
      if (!chat_turns.empty()) {
        if (!system_message.empty() || !user_messages.empty()) {
          throw std::invalid_argument("use either --turn or --system/--message for chat history");
        }
        for (const auto& turn : chat_turns) {
          const auto separator = turn.find(':');
          if (separator == std::string::npos || separator == 0 || separator + 1 == turn.size()) {
            throw std::invalid_argument("chat turn must use role:content syntax");
          }
          session.add(parse_chat_role(std::string_view(turn).substr(0, separator)),
                      turn.substr(separator + 1));
        }
      } else {
        if (!system_message.empty()) {
          session.add(llcl::Role::system, system_message);
        }
        for (const auto& message : user_messages) {
          session.add(llcl::Role::user, message);
        }
      }
      if (session.messages().empty()) {
        throw std::invalid_argument("chat requires at least one --message or --turn");
      }
      if (chat_max_tokens >= static_cast<std::int32_t>(config.generation_model.context_size)) {
        throw std::invalid_argument("chat max_tokens must be smaller than context_size");
      }
      const auto max_prompt_tokens =
          config.generation_model.context_size - static_cast<std::size_t>(chat_max_tokens);
      llcl::PromptBuilder prompt_builder;
      const auto built =
          prompt_builder.build(session.messages(), generator, generator, max_prompt_tokens);
      session.begin_prefill();
      session.begin_decoding();
      generation_prompt = built.text;
      options = {
          .max_tokens = chat_max_tokens,
          .temperature = chat_temperature,
          .top_k = chat_top_k,
          .top_p = chat_top_p,
          .repeat_penalty = chat_repeat_penalty,
          .seed = chat_seed,
      };
      llcl::logger()->info("chat history retained={} discarded={} prompt_tokens={}",
                           built.retained_messages.size(), built.discarded_messages,
                           built.token_count);
    } else {
      generation_prompt = prompt;
      options = {
          .max_tokens = max_tokens,
          .temperature = temperature,
          .top_k = top_k,
          .top_p = top_p,
          .repeat_penalty = repeat_penalty,
          .seed = seed,
      };
    }

    std::string response;
    const auto stats = generator.generate(
        generation_prompt, options,
        [&response](std::string_view token) {
          response.append(token);
          std::cout << token;
          std::cout.flush();
        },
        generation_stop.get_token());
    if (chat_session.has_value()) {
      if (generation_stop.stop_requested()) {
        chat_session->cancel();
      } else {
        chat_session->complete(response);
      }
    }
    interrupt_bridge.request_stop();

    std::cout << '\n';
    std::cerr << std::fixed << std::setprecision(2) << "prompt_tokens=" << stats.prompt_tokens
              << '\n'
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
