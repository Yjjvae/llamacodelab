#include "adapters/vector/vector_file.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unistd.h>

namespace llcl::vector_adapter {
namespace {

constexpr std::array<char, 8> kMagic{'L', 'L', 'C', 'L', 'V', 'E', 'C', '1'};
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kModelHashBytes = 32;
constexpr std::size_t kHeaderBytes = kMagic.size() + 4 + 4 + 8 + kModelHashBytes;

template <typename T> void write_le(std::ostream& output, const T value) {
  static_assert(std::is_unsigned_v<T>);
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    output.put(static_cast<char>((value >> (index * 8U)) & static_cast<T>(0xFFU)));
  }
}

template <typename T> [[nodiscard]] T read_le(std::istream& input) {
  static_assert(std::is_unsigned_v<T>);
  T value{};
  for (std::size_t index = 0; index < sizeof(T); ++index) {
    const auto character = input.get();
    if (character == std::char_traits<char>::eof()) {
      throw std::runtime_error("truncated vector file");
    }
    value |= static_cast<T>(static_cast<unsigned char>(character)) << (index * 8U);
  }
  return value;
}

[[nodiscard]] std::array<char, kModelHashBytes> encode_hash(const std::string_view hash) {
  std::array<char, kModelHashBytes> result{};
  if (hash.size() > result.size()) {
    throw std::invalid_argument("vector model hash exceeds 32 bytes");
  }
  std::memcpy(result.data(), hash.data(), hash.size());
  return result;
}

[[nodiscard]] std::string decode_hash(const std::array<char, kModelHashBytes>& bytes) {
  const auto terminator = std::find(bytes.begin(), bytes.end(), '\0');
  return std::string(bytes.begin(), terminator);
}

void fsync_file(const std::filesystem::path& path) {
  const auto descriptor = ::open(path.c_str(), O_RDONLY);
  if (descriptor < 0) {
    throw std::system_error(errno, std::generic_category(), "cannot open vector file for fsync");
  }
  const auto close_descriptor = [&]() { ::close(descriptor); };
  if (::fsync(descriptor) != 0) {
    const auto error = errno;
    close_descriptor();
    throw std::system_error(error, std::generic_category(), "cannot fsync vector file");
  }
  close_descriptor();
}

} // namespace

void VectorFile::write_atomic(const std::filesystem::path& path, const VectorFileMetadata& metadata,
                              const std::vector<StoredVector>& records) {
  if (metadata.dimension == 0 || metadata.model_hash.empty()) {
    throw std::invalid_argument("vector metadata dimension and model hash are required");
  }
  const auto record_bytes =
      sizeof(std::uint64_t) + static_cast<std::size_t>(metadata.dimension) * 4U;
  if (record_bytes < sizeof(std::uint64_t) ||
      records.size() > (std::numeric_limits<std::uint64_t>::max() - kHeaderBytes) / record_bytes) {
    throw std::overflow_error("vector file size overflows uint64");
  }
  std::filesystem::create_directories(path.parent_path());
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw std::runtime_error("cannot create vector temporary file: " + temporary);
  }
  output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
  write_le<std::uint32_t>(output, kVersion);
  write_le<std::uint32_t>(output, metadata.dimension);
  write_le<std::uint64_t>(output, static_cast<std::uint64_t>(records.size()));
  const auto hash = encode_hash(metadata.model_hash);
  output.write(hash.data(), static_cast<std::streamsize>(hash.size()));
  for (const auto& record : records) {
    if (record.values.size() != metadata.dimension) {
      throw std::invalid_argument("vector record dimension does not match metadata");
    }
    write_le<std::uint64_t>(output, record.chunk_id);
    for (const auto value : record.values) {
      write_le<std::uint32_t>(output, std::bit_cast<std::uint32_t>(value));
    }
  }
  output.flush();
  if (!output) {
    throw std::runtime_error("failed to write vector temporary file");
  }
  output.close();
  fsync_file(temporary);
  std::filesystem::rename(temporary, path);
}

std::vector<StoredVector> VectorFile::read(const std::filesystem::path& path,
                                           VectorFileMetadata* metadata) {
  const auto file_size = std::filesystem::file_size(path);
  if (file_size < kHeaderBytes) {
    throw std::runtime_error("vector file is smaller than its header");
  }
  std::ifstream input(path, std::ios::binary);
  std::array<char, 8> magic{};
  input.read(magic.data(), static_cast<std::streamsize>(magic.size()));
  if (magic != kMagic || read_le<std::uint32_t>(input) != kVersion) {
    throw std::runtime_error("unsupported vector file magic or version");
  }
  const auto dimension = read_le<std::uint32_t>(input);
  const auto count = read_le<std::uint64_t>(input);
  std::array<char, kModelHashBytes> hash{};
  input.read(hash.data(), static_cast<std::streamsize>(hash.size()));
  if (!input || dimension == 0) {
    throw std::runtime_error("invalid vector file header");
  }
  const auto record_bytes = sizeof(std::uint64_t) + static_cast<std::uint64_t>(dimension) * 4U;
  if (record_bytes < sizeof(std::uint64_t) ||
      count > (std::numeric_limits<std::uint64_t>::max() - kHeaderBytes) / record_bytes ||
      kHeaderBytes + count * record_bytes != file_size) {
    throw std::runtime_error("vector file size does not match its header");
  }
  if (metadata != nullptr) {
    *metadata = {.dimension = dimension, .model_hash = decode_hash(hash)};
  }
  std::vector<StoredVector> records;
  records.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t index = 0; index < count; ++index) {
    StoredVector record;
    record.chunk_id = read_le<std::uint64_t>(input);
    record.values.reserve(dimension);
    for (std::uint32_t component = 0; component < dimension; ++component) {
      record.values.push_back(std::bit_cast<float>(read_le<std::uint32_t>(input)));
    }
    records.push_back(std::move(record));
  }
  return records;
}

} // namespace llcl::vector_adapter
