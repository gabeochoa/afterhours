#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "../logging.h"
#include "entity.h"
#include "entity_helper.h"
namespace afterhours {

template <typename Derived = void> //
struct EntityQuery {
  using TReturn =
      std::conditional_t<std::is_same_v<Derived, void>, EntityQuery, Derived>;

  using FilterFn = std::function<bool(const Entity &)>;
  using OrderByFn = std::function<bool(const Entity &, const Entity &)>;

  // -----------------------------------------------------------------------
  // Modification base class.
  //
  // This is the original virtual-dispatch filter system. It is kept for
  // backward compatibility so that custom EntityQuery subclasses (e.g.
  // example/core/custom_queries) can still define their own Modification
  // subclasses and register them via add_mod().
  //
  // >>> MIGRATION NOTICE <<<
  // The built-in Modification subclasses (Not, Limit, WhereID, etc.) are
  // deprecated. Prefer using add_filter() with a lambda or std::function
  // instead, which avoids the virtual-dispatch overhead.
  //
  // To opt in to the optimized path now, #define
  // SKIP_ENTITY_QUERY_MODIFICATIONS before including this header. This removes
  // the built-in struct definitions and makes the where*() methods use direct
  // lambdas.
  //
  // Custom Modification subclasses registered via add_mod() continue to
  // work regardless of the define.
  // -----------------------------------------------------------------------
  struct Modification {
    virtual ~Modification() = default;
    virtual bool operator()(const Entity &) const = 0;
  };

  // add_mod: accepts a heap-allocated Modification* and wraps it into a
  // FilterFn so that the internal filter pipeline is always uniform.
  // Custom EntityQuery subclasses should continue to use this.
  TReturn &add_mod(Modification *mod) {
    auto sp = std::shared_ptr<Modification>(mod);
    mods.push_back([sp](const Entity &e) { return (*sp)(e); });
    return static_cast<TReturn &>(*this);
  }

  // add_filter: preferred way to register filters in new code.
  TReturn &add_filter(FilterFn fn) {
    mods.push_back(std::move(fn));
    return static_cast<TReturn &>(*this);
  }

  // -----------------------------------------------------------------------
  // Built-in Modification subclasses (backward-compatible, virtual path).
  // Define SKIP_ENTITY_QUERY_MODIFICATIONS to remove these and use the
  // direct-lambda fast path instead.
  // -----------------------------------------------------------------------
#ifndef SKIP_ENTITY_QUERY_MODIFICATIONS

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

  TReturn &
  whereLambdaExistsAndTrue(const std::function<bool(const Entity &)> &fn) {
    if (fn)
      return add_mod(new WhereLambda(fn));
    return static_cast<TReturn &>(*this);
  }

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
    if (orderby) {
      log_error("We only apply the first order by in a query at the moment");
      return static_cast<TReturn &>(*this);
    }
    orderby = sortfn;
    return static_cast<TReturn &>(*this);
  }

#else // SKIP_ENTITY_QUERY_MODIFICATIONS -- optimized direct-lambda path

  TReturn &take(const int amount) {
    auto taken = std::make_shared<int>(0);
    return add_filter([amount, taken](const Entity &) {
      if (*taken > amount)
        return false;
      (*taken)++;
      return true;
    });
  }
  TReturn &first() { return take(1); }

  TReturn &whereID(const int id) {
    return add_filter([id](const Entity &e) { return e.id == id; });
  }
  TReturn &whereNotID(const int id) {
    return add_filter([id](const Entity &e) { return e.id != id; });
  }

  TReturn &whereMarkedForCleanup() {
    return add_filter([](const Entity &e) { return e.cleanup; });
  }
  TReturn &whereNotMarkedForCleanup() {
    return add_filter([](const Entity &e) { return !e.cleanup; });
  }

  template <typename T> auto &whereHasComponent() {
    return add_filter([](const Entity &e) { return e.has<T>(); });
  }
  template <typename T> auto &whereMissingComponent() {
    return add_filter([](const Entity &e) { return !e.has<T>(); });
  }

  TReturn &whereLambda(const std::function<bool(const Entity &)> &fn) {
    return add_filter(fn);
  }

  TReturn &whereHasTag(const TagId id) {
    return add_filter([id](const Entity &e) { return e.hasTag(id); });
  }

  TReturn &whereHasAllTags(const TagBitset &mask) {
    return add_filter([mask](const Entity &e) { return e.hasAllTags(mask); });
  }

  TReturn &whereHasAnyTag(const TagBitset &mask) {
    return add_filter([mask](const Entity &e) { return e.hasAnyTag(mask); });
  }

  TReturn &whereHasNoTags(const TagBitset &mask) {
    return add_filter([mask](const Entity &e) { return e.hasNoTags(mask); });
  }

  TReturn &
  whereLambdaExistsAndTrue(const std::function<bool(const Entity &)> &fn) {
    if (fn)
      return add_filter(fn);
    return static_cast<TReturn &>(*this);
  }

  TReturn &orderByLambda(const OrderByFn &sortfn) {
    if (orderby) {
      log_error("We only apply the first order by in a query at the moment");
      return static_cast<TReturn &>(*this);
    }
    orderby = sortfn;
    return static_cast<TReturn &>(*this);
  }

#endif // SKIP_ENTITY_QUERY_MODIFICATIONS

  // -----------------------------------------------------------------------
  // Template tag overloads (shared by both paths, delegate to TagId/Bitset
  // versions defined above).
  // -----------------------------------------------------------------------
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

  struct UnderlyingOptions {
    bool stop_on_first = false;
  };

  [[nodiscard]] bool has_values() const {
    // If we've already materialized results, don't re-run.
    if (ran_query)
      return !ents.empty();
    return !run_query({.stop_on_first = true}).empty();
  }

  [[nodiscard]] bool is_empty() const { return !has_values(); }

  [[nodiscard]] RefEntities
  values_ignore_cache(const UnderlyingOptions options) const {
    ents = run_query(options);
    ran_query = true;
    return ents;
  }

  [[nodiscard]] RefEntities gen() const {
    if (!ran_query)
      return values_ignore_cache({});
    return ents;
  }

  template <typename Component>
  [[nodiscard]] std::vector<std::reference_wrapper<Component>> gen_as() const {
    const auto results = gen();
    std::vector<std::reference_wrapper<Component>> components;
    components.reserve(results.size());
    for (Entity &entity : results) {
      components.push_back(std::ref(entity.get<Component>()));
    }
    return components;
  }

  [[nodiscard]] RefEntities
  gen_with_options(const UnderlyingOptions options) const {
    if (!ran_query)
      return values_ignore_cache(options);
    return ents;
  }

  [[nodiscard]] OptEntity gen_first() const {
    // Avoid calling has_values() here; that would run the query twice.
    const auto values = run_query({.stop_on_first = true});
    if (values.empty())
      return {};
    return values[0];
  }

  [[nodiscard]] Entity &gen_first_enforce() const {
    const auto values = run_query({.stop_on_first = true});
    if (values.empty()) {
      log_error("tried to use gen_first_enforce, but found no values");
    }
    return values[0];
  }

  template <typename Component> [[nodiscard]] Component &gen_first_as() const {
    return gen_first_enforce().template get<Component>();
  }

  [[nodiscard]] std::optional<int> gen_first_id() const {
    auto opt = gen_first();
    if (!opt)
      return {};
    return opt.asE().id;
  }

  // Generate stable handles for the current result set.
  // This is useful for storing long-lived references without keeping pointers.
  [[nodiscard]] std::vector<EntityHandle> gen_handles() const {
    const auto results = gen();
    std::vector<EntityHandle> handles;
    handles.reserve(results.size());
    for (Entity &ent : results) {
      handles.push_back(EntityHelper::handle_for(ent));
    }
    return handles;
  }

  // Generate the first stable handle for this query (if any).
  // Returns empty if the query returns no entities or if the entity does not
  // have a valid handle (e.g., temp pre-merge).
  [[nodiscard]] std::optional<EntityHandle> gen_first_handle() const {
    auto opt = gen_first();
    if (!opt)
      return {};
    EntityHandle h = EntityHelper::handle_for(opt.asE());
    if (h.is_invalid())
      return {};
    return h;
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
      : EntityQuery(EntityHelper::get_default_collection(), options) {}

  explicit EntityQuery(EntityCollection &collection,
                       const QueryOptions &options = {})
      : entities(init_entities_ref(collection, options)) {
    const size_t size = collection.get_temp().size();
    if (size == 0)
      return;

    if (!options.force_merge && !options.ignore_temp_warning) {
      const auto &temp_entities = collection.get_temp();
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
  explicit EntityQuery(const Entities &entsIn) : entities(entsIn) {}

private:
  const Entities &entities;

  // Helper to initialize the entities reference in the constructor.
  // If force_merge is requested, merge first, then return the (now-updated)
  // reference. Otherwise just return the current entities.
  static const Entities &init_entities_ref(EntityCollection &collection,
                                           const QueryOptions &options) {
    if (options.force_merge && !collection.get_temp().empty()) {
      collection.merge_entity_arrays();
    }
    return collection.get_entities();
  }

  std::optional<OrderByFn> orderby;
  std::vector<FilterFn> mods;
  mutable RefEntities ents;
  mutable bool ran_query = false;

  RefEntities run_query(const UnderlyingOptions options) const {
    // Fast path: if we only need to know whether *any* entity matches (or
    // to return the first match), we can short-circuit as long as we aren't
    // ordering results. No upfront allocation needed.
    if (options.stop_on_first && !orderby) {
      for (const auto &e_ptr : entities) {
        if (!e_ptr)
          continue;
        const Entity &e = *e_ptr;
        bool ok = true;
        for (const auto &mod : mods) {
          if (!mod(e)) {
            ok = false;
            break;
          }
        }
        if (ok) {
          RefEntities result;
          result.push_back(const_cast<Entity &>(e));
          return result;
        }
      }
      return {};
    }

    RefEntities out;
    out.reserve(mods.empty() ? entities.size() : entities.size() / 2);

    for (const auto &e_ptr : entities) {
      if (!e_ptr)
        continue;
      Entity &e = *e_ptr;
      bool ok = true;
      for (const auto &mod : mods) {
        if (!mod(e)) {
          ok = false;
          break;
        }
      }
      if (ok)
        out.push_back(e);
    }

    if (orderby && out.size() > 1) {
      std::sort(out.begin(), out.end(), [&](const Entity &a, const Entity &b) {
        return (*orderby)(a, b);
      });
    }

    return out;
  }
};
} // namespace afterhours
