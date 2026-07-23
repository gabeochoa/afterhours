#pragma once

#include <chrono>
#include <cctype>
#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>

#if __has_include("httplib.h")
#include "httplib.h"
#define AFTERHOURS_HAS_CPP_HTTPLIB 1
#endif

#include "../core/base_component.h"
#include "../core/entity_helper.h"
#include "../core/system.h"
#include "../developer.h"
#include "../logging.h"

namespace afterhours {

using TelemetryFields = std::unordered_map<std::string, std::string>;

template<typename T>
struct telemetry_traits;

struct TelemetryProvider {
  virtual ~TelemetryProvider() = default;
  virtual void init() {}
  virtual bool send_get(const std::string &endpoint,
                        const TelemetryFields &query) = 0;
  virtual void flush() {}
};

struct telemetry : developer::Plugin {
  struct Config {
    bool enabled = true;
    std::string endpoint;
    std::string game_version;
    std::string platform;
    std::string schema_version = "1";
    size_t max_queue = 256;
    int max_retries = 3;
  };

  struct ProvidesTelemetry : BaseComponent {
    Config config;
    std::string session_id;
    uint64_t dropped_events = 0;
  };

  static void add_singleton_components(Entity &entity) {
    entity.addComponent<ProvidesTelemetry>();
    EntityHelper::registerSingleton<ProvidesTelemetry>(entity);
  }
  static void enforce_singletons(SystemManager &sm) {
    sm.register_update_system(
        std::make_unique<developer::EnforceSingleton<ProvidesTelemetry>>());
  }
  static void register_update_systems(SystemManager &sm);

  static void init(const Config &cfg,
                   std::unique_ptr<TelemetryProvider> injected_provider);
  static void shutdown();

  // Escape hatch for non-typed experimentation.
  static void track_custom(std::string_view event_name,
                           const TelemetryFields &fields);

  template<typename T>
  static void track(const T &event) {
    static_assert(
        requires {
          telemetry_traits<T>::name;
          telemetry_traits<T>::to_fields(event);
        },
        "Telemetry type must define telemetry_traits<T>::name and "
        "telemetry_traits<T>::to_fields(const T&)");
    track_custom(telemetry_traits<T>::name,
                 telemetry_traits<T>::to_fields(event));
  }
};

struct NoopTelemetryProvider : TelemetryProvider {
  bool send_get(const std::string &, const TelemetryFields &) override {
    return true;
  }
};

// Compile-time verification that telemetry satisfies PluginCore concept.
static_assert(developer::PluginCore<telemetry>,
              "telemetry must implement the core plugin interface");

namespace telemetry_detail {
inline std::string url_encode(std::string_view input) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(input.size() * 3);
  for (unsigned char c : input) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
      continue;
    }
    out.push_back('%');
    out.push_back(hex[c >> 4]);
    out.push_back(hex[c & 0x0F]);
  }
  return out;
}

inline std::string build_query_string(const TelemetryFields &query) {
  std::string out;
  bool first = true;
  for (const auto &[k, v] : query) {
    if (!first) out += "&";
    first = false;
    out += url_encode(k);
    out += "=";
    out += url_encode(v);
  }
  return out;
}

struct EndpointParts {
  std::string host;
  std::string path;
  int port = 80;
};

inline bool parse_http_endpoint(const std::string &endpoint, EndpointParts &out) {
  static constexpr std::string_view kHttpPrefix = "http://";
  if (!endpoint.starts_with(kHttpPrefix)) return false;

  const std::string_view rest =
      std::string_view(endpoint).substr(kHttpPrefix.size());
  const size_t slash = rest.find('/');
  const std::string_view host_port =
      (slash == std::string_view::npos) ? rest : rest.substr(0, slash);
  if (host_port.empty()) return false;

  out.path =
      (slash == std::string_view::npos) ? "/" : std::string(rest.substr(slash));
  out.host.clear();
  out.port = 80;

  const size_t colon = host_port.find(':');
  if (colon == std::string_view::npos) {
    out.host = std::string(host_port);
    return true;
  }

  out.host = std::string(host_port.substr(0, colon));
  if (out.host.empty()) return false;
  const std::string port_text = std::string(host_port.substr(colon + 1));
  if (port_text.empty()) return false;

  try {
    out.port = std::stoi(port_text);
  } catch (...) {
    return false;
  }
  return out.port > 0;
}

struct QueuedEvent {
  std::string name;
  TelemetryFields fields;
  int retries = 0;
  int64_t retry_at_ms = 0;
};

inline std::deque<QueuedEvent> &queue() {
  static std::deque<QueuedEvent> q;
  return q;
}

inline std::unique_ptr<TelemetryProvider> &provider() {
  static std::unique_ptr<TelemetryProvider> p;
  return p;
}

inline int64_t now_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

inline std::string random_session_id() {
  static std::mt19937_64 rng{std::random_device{}()};
  std::stringstream ss;
  ss << std::hex << rng() << rng();
  return ss.str();
}

inline std::string detect_platform() {
#if defined(__APPLE__)
  return "mac";
#elif defined(_WIN32)
  return "windows";
#elif defined(__linux__)
  return "linux";
#else
  return "unknown";
#endif
}

inline TelemetryFields build_query(const telemetry::ProvidesTelemetry &state,
                                   const QueuedEvent &e) {
  TelemetryFields q = e.fields;
  q["event"] = e.name;
  q["ts"] = std::to_string(now_ms());
  q["session"] = state.session_id;
  q["ver"] = state.config.game_version;
  q["platform"] = state.config.platform;
  q["schema_version"] = state.config.schema_version;
  return q;
}

}  // namespace telemetry_detail

struct HttpGetTelemetryProvider : TelemetryProvider {
  bool send_get(const std::string &endpoint,
                const TelemetryFields &query) override {
    telemetry_detail::EndpointParts parsed;
    if (!telemetry_detail::parse_http_endpoint(endpoint, parsed)) {
      log_warn("telemetry(HttpGet): unsupported endpoint {}", endpoint);
      return false;
    }

    const std::string request_path =
        parsed.path + "?" + telemetry_detail::build_query_string(query);

#if defined(AFTERHOURS_HAS_CPP_HTTPLIB)
    httplib::Client client(parsed.host, parsed.port);
    client.set_connection_timeout(1, 0);
    client.set_read_timeout(1, 0);
    client.set_write_timeout(1, 0);
    auto res = client.Get(request_path);
    if (!res) {
      log_warn("telemetry(HttpGet): request failed host={} port={} path={}",
               parsed.host, parsed.port, parsed.path);
      return false;
    }
    if (res->status >= 200 && res->status < 300) return true;
    log_warn("telemetry(HttpGet): response status={} endpoint={}", res->status,
             endpoint);
    return false;
#else
    log_warn(
        "telemetry(HttpGet): cpp-httplib unavailable; add httplib.h to include path");
    (void)request_path;
    return false;
#endif
  }
};

namespace telemetry_detail {
struct TelemetryFlushSystem : System<telemetry::ProvidesTelemetry> {
  void for_each_with(Entity &, telemetry::ProvidesTelemetry &state,
                     const float) override {
    if (!state.config.enabled) return;
    if (!provider()) return;

    const int64_t t = now_ms();
    auto &q = queue();
    size_t i = 0;
    while (i < q.size()) {
      QueuedEvent &ev = q[i];
      if (ev.retry_at_ms > t) {
        i++;
        continue;
      }

      const bool ok =
          provider()->send_get(state.config.endpoint, build_query(state, ev));
      if (ok) {
        q.erase(q.begin() + static_cast<long>(i));
        continue;
      }

      ev.retries++;
      if (ev.retries > state.config.max_retries) {
        q.erase(q.begin() + static_cast<long>(i));
        continue;
      }

      int delay_ms = 10000;
      if (ev.retries == 1) delay_ms = 1000;
      if (ev.retries == 2) delay_ms = 3000;
      ev.retry_at_ms = t + delay_ms;
      i++;
    }
  }
};
}  // namespace telemetry_detail

inline void telemetry::register_update_systems(SystemManager &sm) {
  sm.register_update_system(
      std::make_unique<telemetry_detail::TelemetryFlushSystem>());
}

inline void telemetry::init(
    const Config &cfg,
    std::unique_ptr<TelemetryProvider> injected_provider) {
  if (EntityHelper::get_default_collection().has_singleton<ProvidesTelemetry>())
    return;

  Entity &entity = EntityHelper::createPermanentEntity();
  add_singleton_components(entity);
  EntityHelper::merge_entity_arrays();

  auto *t = EntityHelper::get_singleton_cmp<ProvidesTelemetry>();
  if (!t) return;

  t->config = cfg;
  if (t->config.platform.empty())
    t->config.platform = telemetry_detail::detect_platform();
  t->session_id = telemetry_detail::random_session_id();

  telemetry_detail::provider() = std::move(injected_provider);
  if (telemetry_detail::provider()) telemetry_detail::provider()->init();
}

inline void telemetry::shutdown() {
  if (telemetry_detail::provider()) telemetry_detail::provider()->flush();
  telemetry_detail::provider().reset();
  telemetry_detail::queue().clear();
}

inline void telemetry::track_custom(std::string_view event_name,
                                    const TelemetryFields &fields) {
  auto *t = EntityHelper::get_singleton_cmp<ProvidesTelemetry>();
  if (!t || !t->config.enabled) return;
  if (event_name.empty()) return;

  auto &q = telemetry_detail::queue();
  if (q.size() >= t->config.max_queue) {
    q.pop_front();
    t->dropped_events++;
  }

  q.push_back(telemetry_detail::QueuedEvent{
      .name = std::string(event_name), .fields = fields, .retries = 0});
}

}  // namespace afterhours
