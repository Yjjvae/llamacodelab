#include "adapters/filesystem/text_chunker.hpp"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <string>

namespace {
[[nodiscard]] std::size_t argument_or(const int argc, char** argv, const int position,
                                      const std::size_t fallback) {
  return argc <= position ? fallback : static_cast<std::size_t>(std::stoull(argv[position]));
}
} // namespace

int main(int argc, char** argv) {
  const auto lines = argument_or(argc, argv, 1, 10'000);
  const auto iterations = argument_or(argc, argv, 2, 50);
  std::string text;
  text.reserve(lines * 48U);
  for (std::size_t line = 0; line < lines; ++line) {
    text += "int value_" + std::to_string(line) + " = " + std::to_string(line) + ";\n";
  }
  llcl::filesystem_adapter::TextChunker chunker;
  std::size_t chunks{};
  const auto started = std::chrono::steady_clock::now();
  for (std::size_t iteration = 0; iteration < iterations; ++iteration) {
    chunks += chunker.chunk_text("fixture.cpp", "cpp", text).size();
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
          .count();
  std::cout << "{\"benchmark\":\"chunker\",\"seed\":0,\"lines\":" << lines
            << ",\"iterations\":" << iterations << ",\"chunks\":" << chunks
            << ",\"elapsed_ms\":" << elapsed << "}\n";
}
