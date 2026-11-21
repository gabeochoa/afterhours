// Versioned storage codecs: round-trip, migrations, and the typed
// Corrupt / TooNew split (docs gap #11; Pharmasea G-P18 envelope names).
#include <afterhours/src/plugins/storage_codec.h>

#include <charconv>
#include <cstdio>
#include <optional>
#include <string>

using afterhours::storage::Codec;
using afterhours::storage::DecodeStatus;

namespace {

int failures = 0;
void check(bool ok, const char *what) {
  if (!ok) {
    ++failures;
    std::fprintf(stderr, "FAIL: %s\n", what);
  }
}

struct Prefs {
  int volume = 0;
  int brightness = 0; // added in v2; v1 files have no such field
  bool operator==(const Prefs &) const = default;
};

// v2 payload grammar: "volume=<n>;brightness=<n>"
std::optional<Prefs> decode_prefs(std::string_view text) {
  Prefs out;
  bool have_volume = false, have_brightness = false;
  while (!text.empty()) {
    const auto semi = text.find(';');
    const auto field = text.substr(0, semi);
    const auto eq = field.find('=');
    if (eq == std::string_view::npos) return std::nullopt;
    int value = 0;
    const auto num = field.substr(eq + 1);
    const auto r =
        std::from_chars(num.data(), num.data() + num.size(), value);
    if (r.ec != std::errc{} || r.ptr != num.data() + num.size())
      return std::nullopt;
    if (field.substr(0, eq) == "volume") {
      out.volume = value;
      have_volume = true;
    } else if (field.substr(0, eq) == "brightness") {
      out.brightness = value;
      have_brightness = true;
    } else {
      return std::nullopt;
    }
    if (semi == std::string_view::npos) break;
    text.remove_prefix(semi + 1);
  }
  if (!have_volume || !have_brightness) return std::nullopt;
  return out;
}

Codec<Prefs> prefs_codec() {
  return Codec<Prefs>(
      2,
      [](const Prefs &p) {
        return "volume=" + std::to_string(p.volume) +
               ";brightness=" + std::to_string(p.brightness);
      },
      decode_prefs,
      {// v1 -> v2: brightness did not exist; default it.
       [](std::string_view v1) -> std::optional<std::string> {
         if (v1.rfind("volume=", 0) != 0) return std::nullopt;
         return std::string(v1) + ";brightness=100";
       }});
}

} // namespace

int main() {
  const auto codec = prefs_codec();

  // Round-trip at the current version.
  const Prefs want{7, 42};
  const auto encoded = codec.encode(want);
  check(afterhours::storage::peek_version(encoded) == 2, "peek_version");
  const auto round = codec.decode(encoded);
  check(round.status == DecodeStatus::Ok, "round-trip status Ok");
  check(round.ok() && !round.migrated(), "round-trip ok/not migrated");
  check(round.value && *round.value == want, "round-trip value");
  check(round.found_version == 2 && round.current_version == 2,
        "round-trip versions");

  // v1 -> v2 migration: the exact file a pre-brightness build wrote.
  const std::string v1 = "afterhours-storage-codec/1\n1\nvolume=5";
  const auto migrated = codec.decode(v1);
  check(migrated.status == DecodeStatus::Migrated, "v1 status Migrated");
  check(migrated.value && *migrated.value == Prefs{5, 100},
        "v1 migrated value gets the v2 default");
  check(migrated.found_version == 1, "v1 found_version");
  // Re-saving a migrated value advances the file to the current version.
  check(migrated.value &&
            codec.decode(codec.encode(*migrated.value)).status ==
                DecodeStatus::Ok,
        "migrated value re-encodes as current");

  // Corrupt: not an envelope, bad version, and payload the decoder
  // rejects must all be Corrupt (and carry no value).
  for (const std::string bad :
       {"", "volume=5", "afterhours-storage-codec/1\nx\nvolume=5",
        "afterhours-storage-codec/1\n0\nvolume=5",
        "afterhours-storage-codec/1\n2\ngarbage",
        "afterhours-storage-codec/1\n2\nvolume=5" /* missing v2 field */}) {
    const auto r = codec.decode(bad);
    check(r.status == DecodeStatus::Corrupt && !r.value,
          "corrupt input is Corrupt with no value");
  }

  // TooNew: a newer build's file must not reach the decoder at all.
  const auto too_new =
      codec.decode("afterhours-storage-codec/1\n3\nvolume=9;brightness=9");
  check(too_new.status == DecodeStatus::TooNew, "v3 status TooNew");
  check(!too_new.ok() && !too_new.value, "TooNew carries no value");
  check(too_new.found_version == 3, "TooNew found_version");

  // Missing migration step is Corrupt, never a silent skip.
  const Codec<Prefs> no_migration(
      2, [](const Prefs &p) { return std::to_string(p.volume); },
      decode_prefs);
  check(no_migration.decode(v1).status == DecodeStatus::Corrupt,
        "missing migration is Corrupt");

  // Ordered multi-step chain (v1 -> v2 -> v3) via the string codec.
  auto strings = afterhours::storage::make_string_codec(
      3, {[](std::string_view s) -> std::optional<std::string> {
            return std::string(s) + "|v2";
          },
          [](std::string_view s) -> std::optional<std::string> {
            return std::string(s) + "|v3";
          }});
  const auto chained =
      strings.decode("afterhours-storage-codec/1\n1\nbase");
  check(chained.status == DecodeStatus::Migrated &&
            chained.value == "base|v2|v3",
        "migration chain runs in order");
  // A failing migration mid-chain is Corrupt.
  auto broken = afterhours::storage::make_string_codec(
      2, {[](std::string_view) -> std::optional<std::string> {
        return std::nullopt;
      }});
  check(broken.decode("afterhours-storage-codec/1\n1\nbase").status ==
            DecodeStatus::Corrupt,
        "failing migration is Corrupt");

  // Binary payloads (bitsery-style) survive: only the first two
  // newlines are envelope, the rest is verbatim.
  const std::string binary("a\nb\0c", 5);
  const auto bin_codec = afterhours::storage::make_string_codec(1);
  const auto bin = bin_codec.decode(bin_codec.encode(binary));
  check(bin.ok() && bin.value == binary, "binary payload round-trips");

  if (failures == 0) std::printf("storage_codec_test: all passed\n");
  return failures ? 1 : 0;
}
