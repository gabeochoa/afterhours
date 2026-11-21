#pragma once

// Level plugin core: a typed entity registry, an in-memory loader, and
// write-back. A level is a list of Records the caller already parsed;
// folder walking and any file format are the caller's job.
//
//   level::Registry reg;
//   reg.register_type("crate",
//       [](Entity &e, const level::Record &r) {
//         e.addComponent<Position>(r.get_float("x", 0.f), r.get_float("y", 0.f));
//       },
//       [](const Entity &e, level::Record &r) {
//         r.set("x", e.get<Position>().x); r.set("y", e.get<Position>().y);
//       });
//   auto loaded = reg.load(collection, records);  // loaded.level.entities
//   auto out = reg.write_back(loaded.level);     // mutated values, as Records

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "../core/entity_collection.h"
#include "../logging.h"

namespace afterhours::level {

using Value = std::variant<std::int64_t, double, bool, std::string>;

struct Record {
  std::string type;
  std::map<std::string, Value> fields;

  void set(const std::string &name, std::int64_t v) { fields[name] = v; }
  void set(const std::string &name, int v) { fields[name] = std::int64_t{v}; }
  void set(const std::string &name, double v) { fields[name] = v; }
  void set(const std::string &name, float v) { fields[name] = double{v}; }
  void set(const std::string &name, bool v) { fields[name] = v; }
  void set(const std::string &name, const std::string &v) { fields[name] = v; }
  void set(const std::string &name, const char *v) { fields[name] = std::string{v}; }

  [[nodiscard]] std::optional<std::int64_t> get_int(const std::string &n) const {
    auto it = fields.find(n);
    if (it == fields.end()) return std::nullopt;
    if (auto *p = std::get_if<std::int64_t>(&it->second)) return *p;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<double> get_double(const std::string &n) const {
    auto it = fields.find(n);
    if (it == fields.end()) return std::nullopt;
    if (auto *p = std::get_if<double>(&it->second)) return *p;
    if (auto *p = std::get_if<std::int64_t>(&it->second)) return double(*p);
    return std::nullopt;
  }
  [[nodiscard]] std::optional<bool> get_bool(const std::string &n) const {
    auto it = fields.find(n);
    if (it == fields.end()) return std::nullopt;
    if (auto *p = std::get_if<bool>(&it->second)) return *p;
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::string> get_string(const std::string &n) const {
    auto it = fields.find(n);
    if (it == fields.end()) return std::nullopt;
    if (auto *p = std::get_if<std::string>(&it->second)) return *p;
    return std::nullopt;
  }
  [[nodiscard]] std::int64_t get_int(const std::string &n, std::int64_t fb) const {
    return get_int(n).value_or(fb);
  }
  [[nodiscard]] float get_float(const std::string &n, float fb) const {
    auto v = get_double(n);
    return v ? float(*v) : fb;
  }
  [[nodiscard]] bool get_bool(const std::string &n, bool fb) const {
    return get_bool(n).value_or(fb);
  }
  [[nodiscard]] std::string get_string(const std::string &n, std::string fb) const {
    return get_string(n).value_or(std::move(fb));
  }
};

enum class ErrorKind { UnknownType, DuplicateType, EmptyTypeName, NoWriter };

struct Error {
  ErrorKind kind = ErrorKind::UnknownType;
  std::string type;
  std::size_t index = 0; // record index on load, entity index on write-back
  std::string message;
};

struct LoadedLevel {
  struct Entry {
    Entity *entity = nullptr;
    std::string type;
  };
  std::vector<Entry> entries;
  [[nodiscard]] std::size_t size() const { return entries.size(); }
  [[nodiscard]] bool empty() const { return entries.empty(); }
};

struct LoadResult {
  LoadedLevel level;
  std::vector<Error> errors;
  [[nodiscard]] bool ok() const { return errors.empty(); }
};

struct WriteResult {
  std::vector<Record> records;
  std::vector<Error> errors;
  [[nodiscard]] bool ok() const { return errors.empty(); }
};

class Registry {
public:
  using Construct = std::function<void(Entity &, const Record &)>;
  using Write = std::function<void(const Entity &, Record &)>;

  // False (and no change) on an empty name or a name already registered.
  bool register_type(std::string name, Construct construct, Write write) {
    if (name.empty() || !construct) return false;
    if (types_.contains(name)) return false;
    types_.emplace(std::move(name), Type{std::move(construct), std::move(write)});
    return true;
  }
  [[nodiscard]] bool has(const std::string &name) const { return types_.contains(name); }
  [[nodiscard]] std::size_t size() const { return types_.size(); }
  void clear() { types_.clear(); }

  // Unknown types are collected as typed errors and skipped; the rest load.
  [[nodiscard]] LoadResult load(EntityCollection &collection,
                                const std::vector<Record> &records) const {
    LoadResult out;
    out.level.entries.reserve(records.size());
    for (std::size_t i = 0; i < records.size(); ++i) {
      const Record &rec = records[i];
      auto it = types_.find(rec.type);
      if (it == types_.end()) {
        out.errors.push_back({ErrorKind::UnknownType, rec.type, i,
                              "unregistered level entity type: " + rec.type});
        log_error("level: {}", out.errors.back().message);
        continue;
      }
      Entity &e = collection.createEntity();
      it->second.construct(e, rec);
      out.level.entries.push_back({&e, rec.type});
    }
    return out;
  }

  // Entities flagged for cleanup are skipped; they are gone, not saved.
  [[nodiscard]] WriteResult write_back(const LoadedLevel &loaded) const {
    WriteResult out;
    for (std::size_t i = 0; i < loaded.entries.size(); ++i) {
      const auto &entry = loaded.entries[i];
      if (!entry.entity || entry.entity->cleanup) continue;
      auto it = types_.find(entry.type);
      if (it == types_.end()) {
        out.errors.push_back({ErrorKind::UnknownType, entry.type, i,
                              "unregistered level entity type: " + entry.type});
        continue;
      }
      if (!it->second.write) {
        out.errors.push_back({ErrorKind::NoWriter, entry.type, i,
                              "no write-back for level entity type: " + entry.type});
        continue;
      }
      Record rec;
      rec.type = entry.type;
      it->second.write(*entry.entity, rec);
      out.records.push_back(std::move(rec));
    }
    return out;
  }

  // Mark every loaded entity for the collection's next cleanup() sweep.
  static void unload(EntityCollection &collection, LoadedLevel &loaded) {
    for (auto &entry : loaded.entries)
      if (entry.entity) collection.markIDForCleanup(entry.entity->id);
    loaded.entries.clear();
  }

private:
  struct Type {
    Construct construct;
    Write write;
  };
  std::unordered_map<std::string, Type> types_;
};

} // namespace afterhours::level
