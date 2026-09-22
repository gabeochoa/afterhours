#pragma once

#include <any>
#include <functional>
#include <memory>
#include <vector>

#include "../../core/system.h"
#include "../../ecs.h"

namespace afterhours {
namespace ui {
namespace imm {

struct ComponentConfig;

using InitHook = std::function<void(void *, Entity &, const ComponentConfig &)>;
using UIExtensionSystemFactory = std::function<std::unique_ptr<SystemBase>()>;

inline std::vector<InitHook> &init_hooks() {
  static std::vector<InitHook> hooks;
  return hooks;
}

inline void register_init_hook(InitHook hook) {
  init_hooks().push_back(std::move(hook));
}

template <typename Ctx>
inline void run_init_hooks(Ctx &ctx, Entity &entity, const ComponentConfig &config) {
  for (const InitHook &hook : init_hooks())
    hook(static_cast<void *>(&ctx), entity, config);
}

inline std::vector<UIExtensionSystemFactory> &ui_extension_system_factories() {
  static std::vector<UIExtensionSystemFactory> factories;
  return factories;
}

inline void register_ui_extension_system(UIExtensionSystemFactory factory) {
  ui_extension_system_factories().push_back(std::move(factory));
}

template <typename T>
inline std::vector<const T *> extensions_all(const ComponentConfig &config);

template <typename T>
inline const T *extensions_of(const ComponentConfig &config) {
  const std::vector<const T *> all = extensions_all<T>(config);
  return all.empty() ? nullptr : all.front();
}

} // namespace imm
} // namespace ui
} // namespace afterhours
