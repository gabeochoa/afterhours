#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../../ecs.h"
#include "../../logging.h"
#include "components.h"

namespace afterhours {

namespace ui {

template <typename Derived, typename DataStorage, typename ProvidedType,
          typename ProviderComponent>
struct ProviderConsumer : public DataStorage {

  struct has_fetch_data_member {
    template <typename U>
    static auto test(U *)
        -> decltype(std::declval<U>().fetch_data(), void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct has_on_data_changed_member {
    template <typename U>
    static auto test(U *) -> decltype(std::declval<U>().on_data_changed(0),
                                      void(), std::true_type{});

    template <typename U> static auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<ProviderComponent>(0))::value;
  };

  struct return_type {
    using type = decltype(std::declval<ProviderComponent>().fetch_data());
  };

  ProviderConsumer(const std::function<void(size_t)> &on_change)
      : DataStorage({{}}, get_data_from_provider, on_change) {}

  virtual void write_value_change_to_provider(const DataStorage &storage) = 0;
  virtual ProvidedType convert_from_fetch(return_type::type data) const = 0;

  static ProvidedType get_data_from_provider(const DataStorage &storage) {
    // log_info("getting data from provider");
    static_assert(std::is_base_of_v<BaseComponent, ProviderComponent>,
                  "ProviderComponent must be a child of BaseComponent");
    // Convert the data in the provider component to a list of options
    auto &provider_entity = EntityQuery()
                                .whereHasComponent<ProviderComponent>()
                                .gen_first_enforce();
    ProviderComponent &pComp =
        provider_entity.template get<ProviderComponent>();

    static_assert(has_fetch_data_member::value,
                  "ProviderComponent must have fetch_data function");

    auto fetch_data_result = pComp.fetch_data();
    return static_cast<const Derived &>(storage).convert_from_fetch(
        fetch_data_result);
  }
};

template <typename ProviderComponent>
struct HasDropdownStateWithProvider
    : public ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent> {

  using PC = ProviderConsumer<HasDropdownStateWithProvider<ProviderComponent>,
                              HasDropdownState, HasDropdownState::Options,
                              ProviderComponent>;

  int max_num_options = -1;
  HasDropdownStateWithProvider(const std::function<void(size_t)> &on_change,
                               int max_options_ = -1)
      : PC(on_change), max_num_options(max_options_) {}

  virtual HasDropdownState::Options
  convert_from_fetch(PC::return_type::type fetched_data) const override {

    // TODO see message above
    // static_assert(is_vector_data_convertible_to_string<
    // decltype(fetch_data_result)>::value, "ProviderComponent::fetch_data must
    // return a vector of items " "convertible to string");
    HasDropdownState::Options new_options;
    for (auto &data : fetched_data) {
      if (max_num_options > 0 && (int)new_options.size() > max_num_options)
        continue;
      new_options.push_back((std::string)data);
    }
    return new_options;
  }

  virtual void
  write_value_change_to_provider(const HasDropdownState &hdswp) override {
    size_t index = hdswp.last_option_clicked;

    auto &entity = EntityQuery()
                       .whereHasComponent<ProviderComponent>()
                       .gen_first_enforce();
    ProviderComponent &pComp = entity.template get<ProviderComponent>();

    static_assert(
        PC::has_on_data_changed_member::value,
        "ProviderComponent must have on_data_changed(index) function");
    pComp.on_data_changed(index);
  }
};

template <typename ProviderComponent>
Entity &make_dropdown(Entity &parent, int max_options = -1) {
  using WRDS = ui::HasDropdownStateWithProvider<ProviderComponent>;
  auto &dropdown = EntityHelper::createEntity();
  dropdown.addComponent<UIComponentDebug>("dropdown");

  dropdown.addComponent<UIComponent>(dropdown.id).set_parent(parent.id);
  parent.get<ui::UIComponent>().add_child(dropdown.id);

  dropdown.addComponent<ui::HasChildrenComponent>();

  dropdown.addComponent<WRDS>(
      [&dropdown](size_t) {
        WRDS &wrds = dropdown.get<WRDS>();
        if (!wrds.on) {
          wrds.write_value_change_to_provider(wrds);
        }
      },
      max_options);

  dropdown.addComponent<HasColor>(colors::UI_BLUE);

  return dropdown;
}

} // namespace ui

} // namespace afterhours
