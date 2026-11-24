#pragma once


#include <charconv>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace afterhours::storage {

enum class DecodeStatus { Ok, Migrated, Corrupt, TooNew };

inline const char *to_string(DecodeStatus status) {
  switch (status) {
  case DecodeStatus::Ok: return "Ok";
  case DecodeStatus::Migrated: return "Migrated";
  case DecodeStatus::Corrupt: return "Corrupt";
  case DecodeStatus::TooNew: return "TooNew";
  }
  return "Corrupt";
}

template <typename T> struct DecodeResult {
  DecodeStatus status = DecodeStatus::Corrupt;
  std::optional<T> value;
  int found_version = 0;
  int current_version = 0;
  std::string message;

  [[nodiscard]] bool ok() const {
    return status == DecodeStatus::Ok || status == DecodeStatus::Migrated;
  }
  [[nodiscard]] bool migrated() const { return status == DecodeStatus::Migrated; }
};

namespace detail {
inline constexpr std::string_view MAGIC = "afterhours-storage-codec/1";

inline std::optional<int> parse_version(std::string_view text) {
  if (text.empty()) return std::nullopt;
  int value = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), value);
  if (r.ec != std::errc{} || r.ptr != text.data() + text.size())
    return std::nullopt;
  return value;
}
struct Envelope {
  int version = 0;
  std::string_view payload;
  std::string_view error;
};

inline Envelope parse_envelope(std::string_view text) {
  const auto first = text.find('\n');
  if (first == std::string_view::npos || text.substr(0, first) != MAGIC)
    return {0, {}, "not a storage-codec envelope"};
  const auto rest = text.substr(first + 1);
  const auto second = rest.find('\n');
  if (second == std::string_view::npos)
    return {0, {}, "envelope has no payload separator"};
  const auto found = parse_version(rest.substr(0, second));
  if (!found || *found < 1)
    return {0, {}, "envelope has a malformed version"};
  return {*found, rest.substr(second + 1), {}};
}
}

inline std::optional<int> peek_version(std::string_view text) {
  const auto env = detail::parse_envelope(text);
  return env.error.empty() ? std::optional<int>(env.version) : std::nullopt;
}

template <typename T> class Codec {
public:
  using EncodeFn = std::function<std::string(const T &)>;
  using DecodeFn = std::function<std::optional<T>(std::string_view)>;
  using MigrationFn =
      std::function<std::optional<std::string>(std::string_view)>;

  Codec(int current_version, EncodeFn encode, DecodeFn decode)
      : version(current_version), encode_fn(std::move(encode)),
        decode_fn(std::move(decode)) {
    if (version < 1)
      throw std::invalid_argument("storage::Codec: version must be >= 1");
    if (!encode_fn || !decode_fn)
      throw std::invalid_argument("storage::Codec: encode/decode required");
  }

  Codec(int current_version, EncodeFn encode, DecodeFn decode,
        std::vector<MigrationFn> migrations)
      : Codec(current_version, std::move(encode), std::move(decode)) {
    for (size_t i = 0; i < migrations.size(); ++i)
      migration(static_cast<int>(i) + 1, std::move(migrations[i]));
  }

  Codec &migration(int from_version, MigrationFn fn) {
    if (from_version < 1 || from_version >= version)
      throw std::invalid_argument(
          "storage::Codec: migration source must be in [1, current)");
    if (!fn)
      throw std::invalid_argument("storage::Codec: empty migration");
    if (!migration_map.emplace(from_version, std::move(fn)).second)
      throw std::invalid_argument(
          "storage::Codec: duplicate migration for a version");
    return *this;
  }

  [[nodiscard]] int current_version() const { return version; }

  [[nodiscard]] std::string encode(const T &value) const {
    const std::string body = encode_fn(value);
    const std::string version_str = std::to_string(version);
    std::string out;
    out.reserve(detail::MAGIC.size() + 1 + version_str.size() + 1 +
                body.size());
    out.append(detail::MAGIC);
    out += '\n';
    out += version_str;
    out += '\n';
    out += body;
    return out;
  }

  [[nodiscard]] DecodeResult<T> decode(std::string_view text) const {
    DecodeResult<T> result;
    result.current_version = version;

    const auto env = detail::parse_envelope(text);
    if (!env.error.empty()) {
      result.message = std::string(env.error);
      return result;
    }
    result.found_version = env.version;
    std::string payload(env.payload);

    if (env.version > version) {
      result.status = DecodeStatus::TooNew;
      result.message = "stored version is newer than this build";
      return result;
    }

    for (int v = env.version; v < version; ++v) {
      const auto it = migration_map.find(v);
      if (it == migration_map.end()) {
        result.message =
            "no migration registered from version " + std::to_string(v);
        return result;
      }
      auto migrated = it->second(payload);
      if (!migrated) {
        result.message =
            "migration from version " + std::to_string(v) + " failed";
        return result;
      }
      payload = std::move(*migrated);
    }

    auto value = decode_fn(payload);
    if (!value) {
      result.message = "payload rejected by the current decoder";
      return result;
    }
    result.value = std::move(*value);
    result.status = env.version == version ? DecodeStatus::Ok
                                           : DecodeStatus::Migrated;
    return result;
  }

private:
  int version;
  EncodeFn encode_fn;
  DecodeFn decode_fn;
  std::map<int, MigrationFn> migration_map;
};

inline Codec<std::string> make_string_codec(
    int current_version,
    std::vector<Codec<std::string>::MigrationFn> migrations = {}) {
  return Codec<std::string>(
      current_version,
      [](const std::string &v) { return v; },
      [](std::string_view payload) {
        return std::optional<std::string>(std::string(payload));
      },
      std::move(migrations));
}

}
