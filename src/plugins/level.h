#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "../core/entity_collection.h"
#include "../logging.h"

namespace afterhours::level {

using Json = nlohmann::json;

enum class ErrorKind { UnknownType, MalformedRecord, InvalidField, NoWriter };

struct Error {
  ErrorKind kind = ErrorKind::UnknownType;
  std::string type;
  std::size_t index = 0;
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
  Json records = Json::array();
  std::vector<Error> errors;

  [[nodiscard]] bool ok() const { return errors.empty(); }
};

class Registry {
public:
  using Construct = std::function<void(Entity &, const Json &)>;
  using Write = std::function<void(const Entity &, Json &)>;

  bool register_type(std::string name, Construct construct, Write write) {
    if (name.empty() || !construct) return false;
    if (types.contains(name)) return false;
    types.emplace(std::move(name), Type{std::move(construct), std::move(write)});
    return true;
  }

  [[nodiscard]] bool has(const std::string &name) const { return types.contains(name); }

  [[nodiscard]] std::size_t size() const { return types.size(); }

  void clear() { types.clear(); }

  [[nodiscard]] LoadResult load(EntityCollection &collection, const Json &records) const {
    LoadResult out;
    if (!records.is_array()) {
      fail(out.errors, ErrorKind::MalformedRecord, "", 0,
           std::format("level records must be a JSON array, got {}", records.type_name()));
      return out;
    }
    out.level.entries.reserve(records.size());
    for (std::size_t i = 0; i < records.size(); ++i) {
      const Json &rec = records[i];
      if (!rec.is_object()) {
        fail(out.errors, ErrorKind::MalformedRecord, "", i,
             std::format("level record {} must be a JSON object, got {}", i, rec.type_name()));
        continue;
      }
      const auto type_it = rec.find("type");
      if (type_it == rec.end() || !type_it->is_string()) {
        fail(out.errors, ErrorKind::MalformedRecord, "", i,
             std::format("level record {} has no string \"type\" field", i));
        continue;
      }
      const std::string type = type_it->get<std::string>();
      const auto it = types.find(type);
      if (it == types.end()) {
        fail(out.errors, ErrorKind::UnknownType, type, i,
             std::format("unregistered level entity type: {}", type));
        continue;
      }
      Entity &entity = collection.createEntity();
      try {
        it->second.construct(entity, rec);
      } catch (const Json::exception &e) {
        collection.markIDForCleanup(entity.id);
        fail(out.errors, ErrorKind::InvalidField, type, i,
             std::format("level record {} ({}): {}", i, type, e.what()));
        continue;
      }
      out.level.entries.push_back({&entity, type});
    }
    return out;
  }

  [[nodiscard]] WriteResult write_back(const LoadedLevel &loaded) const {
    WriteResult out;
    for (std::size_t i = 0; i < loaded.entries.size(); ++i) {
      const auto &entry = loaded.entries[i];
      if (!entry.entity || entry.entity->cleanup) continue;
      const auto it = types.find(entry.type);
      if (it == types.end()) {
        fail(out.errors, ErrorKind::UnknownType, entry.type, i,
             std::format("unregistered level entity type: {}", entry.type));
        continue;
      }
      if (!it->second.write) {
        fail(out.errors, ErrorKind::NoWriter, entry.type, i,
             std::format("no write-back for level entity type: {}", entry.type));
        continue;
      }
      Json rec = Json::object();
      rec["type"] = entry.type;
      try {
        it->second.write(*entry.entity, rec);
      } catch (const Json::exception &e) {
        fail(out.errors, ErrorKind::InvalidField, entry.type, i,
             std::format("level entity {} ({}): {}", i, entry.type, e.what()));
        continue;
      }
      out.records.push_back(std::move(rec));
    }
    return out;
  }

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

  static void fail(std::vector<Error> &errors, ErrorKind kind, std::string type,
                   std::size_t index, std::string message) {
    errors.push_back({kind, std::move(type), index, std::move(message)});
    log_error("level: {}", errors.back().message);
  }

  std::unordered_map<std::string, Type> types;
};

} // namespace afterhours::level
