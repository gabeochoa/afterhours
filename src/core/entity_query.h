#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../logging.h"
#include "component_fingerprint_storage.h"
#include "entity.h"
#include "entity_helper.h"
namespace afterhours {

template <typename Derived = void> //
struct EntityQuery {
  using TReturn =
      std::conditional_t<std::is_same_v<Derived, void>, EntityQuery, Derived>;

  struct Modification {
    virtual ~Modification() = default;
    virtual bool operator()(const Entity &) const = 0;
  };

  TReturn &add_mod(Modification *mod) {
    mods.push_back(std::unique_ptr<Modification>(mod));
    return static_cast<TReturn &>(*this);
  }

  struct Not : Modification {
    std::unique_ptr<Modification> mod;

    explicit Not(Modification *m) : mod(m) {}

    bool operator()(const Entity &entity) const override {
      return !((*mod)(entity));
    }
  };

  struct Limit : Modification {
    int amount;
    mutable int amount_taken;
    explicit Limit(const int amt) : amount(amt), amount_taken(0) {}

    bool operator()(const Entity &) const override {
      if (amount_taken > amount)
        return false;
      amount_taken++;
      return true;
    }
  };
  TReturn &take(const int amount) { return add_mod(new Limit(amount)); }
  TReturn &first() { return take(1); }

  struct WhereID : Modification {
    int id;
    explicit WhereID(const int idIn) : id(idIn) {}
    bool operator()(const Entity &entity) const override {
      return entity.id == id;
    }
  };
  TReturn &whereID(const int id) { return add_mod(new WhereID(id)); }
  TReturn &whereNotID(const int id) {
    return add_mod(new Not(new WhereID(id)));
  }

  struct WhereMarkedForCleanup : Modification {
    bool operator()(const Entity &entity) const override {
      return entity.cleanup;
    }
  };

  TReturn &whereMarkedForCleanup() {
    return add_mod(new WhereMarkedForCleanup());
  }
  TReturn &whereNotMarkedForCleanup() {
    return add_mod(new Not(new WhereMarkedForCleanup()));
  }

  template <typename T> struct WhereHasComponent : Modification {
    bool operator()(const Entity &entity) const override {
      return entity.has<T>();
    }
  };
  template <typename T> auto &whereHasComponent() {
    // Track component requirement for SOA filtering
    if constexpr (std::is_base_of_v<BaseComponent, T>) {
      ComponentID cid = components::get_type_id<T>();
      required_components_mask[cid] = true;
      has_component_requirements = true;
    }
    return add_mod(new WhereHasComponent<T>());
  }
  template <typename T> auto &whereMissingComponent() {
    return add_mod(new Not(new WhereHasComponent<T>()));
  }

  struct WhereLambda : Modification {
    std::function<bool(const Entity &)> filter;
    explicit WhereLambda(const std::function<bool(const Entity &)> &cb)
        : filter(cb) {}
    bool operator()(const Entity &entity) const override {
      return filter(entity);
    }
  };
  TReturn &whereLambda(const std::function<bool(const Entity &)> &fn) {
    return add_mod(new WhereLambda(fn));
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  auto &whereHasTag(const TEnum tag_enum) {
    return whereHasTag(static_cast<TagId>(tag_enum));
  }

  template <auto TagEnum,
            std::enable_if_t<std::is_enum_v<decltype(TagEnum)>, int> = 0>
  auto &whereHasTag() {
    return whereHasTag(static_cast<TagId>(TagEnum));
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  auto &whereHasAllTags(const TEnum tag_enum) {
    return whereHasTag(static_cast<TagId>(tag_enum));
  }

  template <auto TagEnum,
            std::enable_if_t<std::is_enum_v<decltype(TagEnum)>, int> = 0>
  auto &whereHasAllTags() {
    TagBitset mask;
    mask.set(static_cast<TagId>(TagEnum));
    return whereHasAllTags(mask);
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  auto &whereHasAnyTag(const TEnum tag_enum) {
    TagBitset mask;
    mask.set(static_cast<TagId>(tag_enum));
    return whereHasAnyTag(mask);
  }

  template <auto TagEnum,
            std::enable_if_t<std::is_enum_v<decltype(TagEnum)>, int> = 0>
  auto &whereHasAnyTag() {
    TagBitset mask;
    mask.set(static_cast<TagId>(TagEnum));
    return whereHasAnyTag(mask);
  }

  template <typename TEnum, std::enable_if_t<std::is_enum_v<TEnum>, int> = 0>
  auto &whereHasNoTags(const TEnum tag_enum) {
    TagBitset mask;
    mask.set(static_cast<TagId>(tag_enum));
    return whereHasNoTags(mask);
  }

  template <auto TagEnum,
            std::enable_if_t<std::is_enum_v<decltype(TagEnum)>, int> = 0>
  auto &whereHasNoTags() {
    TagBitset mask;
    mask.set(static_cast<TagId>(TagEnum));
    return whereHasNoTags(mask);
  }
  TReturn &
  whereLambdaExistsAndTrue(const std::function<bool(const Entity &)> &fn) {
    if (fn)
      return add_mod(new WhereLambda(fn));
    return static_cast<TReturn &>(*this);
  }

  struct WhereHasTag : Modification {
    TagId id;
    explicit WhereHasTag(const TagId idIn) : id(idIn) {}
    bool operator()(const Entity &entity) const override {
      return entity.hasTag(id);
    }
  };
  TReturn &whereHasTag(const TagId id) { return add_mod(new WhereHasTag(id)); }

  struct WhereHasAllTags : Modification {
    TagBitset mask;
    explicit WhereHasAllTags(const TagBitset &m) : mask(m) {}
    bool operator()(const Entity &entity) const override {
      return entity.hasAllTags(mask);
    }
  };
  TReturn &whereHasAllTags(const TagBitset &mask) {
    return add_mod(new WhereHasAllTags(mask));
  }

  struct WhereHasAnyTag : Modification {
    TagBitset mask;
    explicit WhereHasAnyTag(const TagBitset &m) : mask(m) {}
    bool operator()(const Entity &entity) const override {
      return entity.hasAnyTag(mask);
    }
  };
  TReturn &whereHasAnyTag(const TagBitset &mask) {
    return add_mod(new WhereHasAnyTag(mask));
  }

  struct WhereHasNoTags : Modification {
    TagBitset mask;
    explicit WhereHasNoTags(const TagBitset &m) : mask(m) {}
    bool operator()(const Entity &entity) const override {
      return entity.hasNoTags(mask);
    }
  };
  TReturn &whereHasNoTags(const TagBitset &mask) {
    return add_mod(new WhereHasNoTags(mask));
  }

  using OrderByFn = std::function<bool(const Entity &, const Entity &)>;
  struct OrderBy {
    virtual ~OrderBy() {}
    virtual bool operator()(const Entity &a, const Entity &b) = 0;
  };

  struct OrderByLambda : OrderBy {
    OrderByFn sortFn;
    explicit OrderByLambda(const OrderByFn &sortFnIn) : sortFn(sortFnIn) {}

    bool operator()(const Entity &a, const Entity &b) override {
      return sortFn(a, b);
    }
  };

  TReturn &orderByLambda(const OrderByFn &sortfn) {
    return static_cast<TReturn &>(set_order_by(new OrderByLambda(sortfn)));
  }

  struct UnderlyingOptions {
    bool stop_on_first = false;
  };

  [[nodiscard]] bool has_values() const {
    return !run_query({.stop_on_first = true}).empty();
  }

  [[nodiscard]] bool is_empty() const {
    return run_query({.stop_on_first = true}).empty();
  }

  [[nodiscard]] RefEntities
  values_ignore_cache(const UnderlyingOptions options) const {
    ents = run_query(options);
    return ents;
  }

  [[nodiscard]] RefEntities gen() const {
    if (!ran_query)
      return values_ignore_cache({});
    return ents;
  }

  [[nodiscard]] RefEntities
  gen_with_options(const UnderlyingOptions options) const {
    if (!ran_query)
      return values_ignore_cache(options);
    return ents;
  }

  [[nodiscard]] OptEntity gen_first() const {
    if (has_values()) {
      const auto values = gen_with_options({.stop_on_first = true});
      if (values.empty()) {
        log_error("we expected to find a value but didnt...");
      }
      return values[0];
    }
    return {};
  }

  [[nodiscard]] Entity &gen_first_enforce() const {
    if (!has_values()) {
      log_error("tried to use gen enforce, but found no values");
    }
    const auto values = gen_with_options({.stop_on_first = true});
    if (values.empty()) {
      log_error("we expected to find a value but didnt...");
    }
    return values[0];
  }

  [[nodiscard]] std::optional<int> gen_first_id() const {
    if (!has_values())
      return {};
    return gen_with_options({.stop_on_first = true})[0].get().id;
  }

  [[nodiscard]] size_t gen_count() const {
    if (!ran_query)
      return values_ignore_cache({}).size();
    return ents.size();
  }

  [[nodiscard]] std::vector<int> gen_ids() const {
    const auto results = ran_query ? ents : values_ignore_cache({});
    std::vector<int> ids;
    std::transform(results.begin(), results.end(), std::back_inserter(ids),
                   [](const Entity &ent) -> int { return ent.id; });
    return ids;
  }

  [[nodiscard]] OptEntity gen_random() const {
    const auto results = gen();
    if (results.empty()) {
      return {};
    }
    size_t random_index = rand() % results.size();
    return results[random_index];
  }

  template <typename RngFunc>
  [[nodiscard]] OptEntity gen_random(RngFunc &&rng_func) const {
    const auto results = gen();
    if (results.empty()) {
      return {};
    }
    size_t random_index = rng_func(results.size());
    if (random_index >= results.size()) {
      return {};
    }
    return results[random_index];
  }

  struct QueryOptions {
    bool force_merge = false;
    bool ignore_temp_warning = false;
  };

  EntityQuery(const QueryOptions &options = {})
      : entities(EntityHelper::get_entities()) {
    const size_t size = EntityHelper::get().temp_entities.size();
    if (size == 0)
      return;

    if (options.force_merge) {
      EntityHelper::merge_entity_arrays();
      entities = EntityHelper::get_entities();
    } else if (!options.ignore_temp_warning) {
      const auto &temp_entities = EntityHelper::get().temp_entities;
      const size_t num_to_print = std::min(size_t(10), temp_entities.size());

      for (size_t i = 0; i < num_to_print; ++i) {
        const auto &entity = temp_entities[i];
        if (entity) {
          log_warn("  temp entity {}: id={}, cleanup={}", i, entity->id,
                   entity->cleanup);
        } else {
          log_warn("  temp entity {}: null", i);
        }
      }
      log_error("query will miss {} ents in temp", size);
    }
  }
  explicit EntityQuery(const Entities &entsIn) : entities(entsIn) {
    entities = entsIn;
  }

private:
  Entities entities;

  std::unique_ptr<OrderBy> orderby;
  std::vector<std::unique_ptr<Modification>> mods;
  mutable RefEntities ents;
  mutable bool ran_query = false;
  
  // Track component requirements for SOA fingerprint filtering
  ComponentBitSet required_components_mask;
  bool has_component_requirements = false;

  EntityQuery &set_order_by(OrderBy *ob) {
    if (orderby) {
      log_error("We only apply the first order by in a query at the moment");
      return static_cast<TReturn &>(*this);
    }
    orderby = std::unique_ptr<OrderBy>(ob);
    return static_cast<TReturn &>(*this);
  }

  RefEntities filter_mod(const RefEntities &in,
                         const std::unique_ptr<Modification> &mod) const {
    RefEntities out;
    out.reserve(in.size());
    for (const auto &entity : in) {
      if ((*mod)(entity)) {
        out.push_back(entity);
      }
    }
    return out;
  }


  // Filter entities by SOA fingerprint first (fast pre-filtering)
  RefEntities filter_by_soa_fingerprints() const {
    RefEntities out;
    
    // If no component requirements, use all entities (backward compatibility)
    if (!has_component_requirements) {
      out.reserve(entities.size());
      for (const auto &e_ptr : entities) {
        if (!e_ptr)
          continue;
        Entity &e = *e_ptr;
        out.push_back(e);
      }
      return out;
    }
    
    // Use SOA fingerprint storage for fast filtering
    const auto &fingerprint_storage = EntityHelper::get_fingerprint_storage();
    const auto &all_fingerprints = fingerprint_storage.fingerprints;
    const auto &all_entity_ids = fingerprint_storage.entity_ids;
    
    // Build lookup map for fast entity access
    std::unordered_map<EntityID, std::shared_ptr<Entity>> entity_map;
    entity_map.reserve(entities.size());
    for (const auto &e_ptr : entities) {
      if (e_ptr) {
        entity_map[e_ptr->id] = e_ptr;
      }
    }
    
    // Filter by fingerprints
    out.reserve(all_fingerprints.size());
    for (size_t i = 0; i < all_fingerprints.size(); ++i) {
      EntityID eid = all_entity_ids[i];
      
      // Skip cleanup-marked entities
      if (fingerprint_storage.cleanup_marked.contains(eid)) {
        continue;
      }
      
      // Check if fingerprint matches required components
      const ComponentBitSet &fp = all_fingerprints[i];
      if ((fp & required_components_mask) == required_components_mask) {
        // Find entity in entities list
        auto it = entity_map.find(eid);
        if (it != entity_map.end()) {
          out.push_back(*(it->second));
        }
      }
    }
    
    return out;
  }

  RefEntities run_query(const UnderlyingOptions) const {
    // Use SOA fingerprint filtering if we have component requirements
    RefEntities out;
    if (has_component_requirements) {
      out = filter_by_soa_fingerprints();
    } else {
      // No component requirements, use all entities (backward compatibility)
      out.reserve(entities.size());
      for (const auto &e_ptr : entities) {
        if (!e_ptr)
          continue;
        Entity &e = *e_ptr;
        out.push_back(e);
      }
    }

    // Apply remaining modifications (tags, lambdas, etc.)
    auto it = out.end();
    for (const auto &mod : mods) {
      it = std::partition(out.begin(), it, [&mod](const auto &entity) {
        return (*mod)(entity);
      });
    }

    out.erase(it, out.end());

    if (out.size() == 1) {
      return out;
    }

    if (orderby) {
      std::sort(out.begin(), out.end(), [&](const Entity &a, const Entity &b) {
        return (*orderby)(a, b);
      });
    }

    return out;
  }
};
} // namespace afterhours
