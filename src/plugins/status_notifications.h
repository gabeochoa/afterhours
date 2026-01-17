// Status Notifications Plugin for Afterhours
// Provides timed status messages/notifications for UI feedback.
// Useful for: "Saved!", "Error: ...", achievement popups, tooltips, etc.
//
// To integrate into Afterhours:
// 1. Add this as a plugin in src/plugins/notifications.h
// 2. Register ProvidesNotifications as a singleton component

#pragma once

#include <deque>
#include <string>
#include <functional>

#include <afterhours/src/core/base_component.h>
#include <afterhours/src/core/entity_helper.h>
#include <afterhours/src/core/system.h>

namespace afterhours {

/// Notification severity levels
enum class NotificationLevel {
  Info,     // Normal information
  Success,  // Positive feedback (saved, completed, etc.)
  Warning,  // Warnings
  Error,    // Errors
};

/// A single notification message
struct Notification {
  std::string message;
  NotificationLevel level = NotificationLevel::Info;
  double created_at = 0.0;    // Time when created
  double duration = 3.0;      // Seconds to display
  double expires_at = 0.0;    // Computed: created_at + duration
  
  bool is_expired(double current_time) const {
    return current_time >= expires_at;
  }
  
  // Factory methods
  static Notification info(const std::string& msg, double duration = 3.0) {
    return Notification{msg, NotificationLevel::Info, 0, duration, 0};
  }
  
  static Notification success(const std::string& msg, double duration = 3.0) {
    return Notification{msg, NotificationLevel::Success, 0, duration, 0};
  }
  
  static Notification warning(const std::string& msg, double duration = 5.0) {
    return Notification{msg, NotificationLevel::Warning, 0, duration, 0};
  }
  
  static Notification error(const std::string& msg, double duration = 7.0) {
    return Notification{msg, NotificationLevel::Error, 0, duration, 0};
  }
};

/// Component that provides notification functionality
struct ProvidesNotifications : BaseComponent {
  std::deque<Notification> notifications;
  std::size_t max_visible = 5;  // Max notifications to show at once
  
  /// Add a notification
  void push(Notification notif, double current_time) {
    notif.created_at = current_time;
    notif.expires_at = current_time + notif.duration;
    notifications.push_back(notif);
    
    // Limit queue size
    while (notifications.size() > max_visible * 2) {
      notifications.pop_front();
    }
  }
  
  /// Convenience methods
  void info(const std::string& msg, double current_time) {
    push(Notification::info(msg), current_time);
  }
  
  void success(const std::string& msg, double current_time) {
    push(Notification::success(msg), current_time);
  }
  
  void warning(const std::string& msg, double current_time) {
    push(Notification::warning(msg), current_time);
  }
  
  void error(const std::string& msg, double current_time) {
    push(Notification::error(msg), current_time);
  }
  
  /// Remove expired notifications
  void cleanup(double current_time) {
    while (!notifications.empty() && 
           notifications.front().is_expired(current_time)) {
      notifications.pop_front();
    }
  }
  
  /// Get visible notifications (not expired)
  std::vector<const Notification*> get_visible(double current_time) const {
    std::vector<const Notification*> result;
    for (const auto& n : notifications) {
      if (!n.is_expired(current_time)) {
        result.push_back(&n);
        if (result.size() >= max_visible) break;
      }
    }
    return result;
  }
  
  /// Check if there are any visible notifications
  bool has_visible(double current_time) const {
    for (const auto& n : notifications) {
      if (!n.is_expired(current_time)) return true;
    }
    return false;
  }
  
  /// Clear all notifications
  void clear() {
    notifications.clear();
  }
};

/// System to cleanup expired notifications
struct NotificationCleanupSystem : System<ProvidesNotifications> {
  void for_each_with(Entity& /*entity*/, ProvidesNotifications& notifs,
                     float /*dt*/) override {
    // Get current time from raylib or your time source
#ifdef AFTER_HOURS_USE_RAYLIB
    double current_time = raylib::GetTime();
#else
    double current_time = 0.0; // Replace with your time source
#endif
    notifs.cleanup(current_time);
  }
};

//-----------------------------------------------------------------------------
// Static API for easy access
//-----------------------------------------------------------------------------

namespace notifications {

/// Get the singleton notifications provider
inline ProvidesNotifications* get_provider() {
  return EntityHelper::get_singleton_cmp<ProvidesNotifications>();
}

/// Show an info notification
inline void info(const std::string& msg) {
  if (auto* p = get_provider()) {
#ifdef AFTER_HOURS_USE_RAYLIB
    p->info(msg, raylib::GetTime());
#endif
  }
}

/// Show a success notification
inline void success(const std::string& msg) {
  if (auto* p = get_provider()) {
#ifdef AFTER_HOURS_USE_RAYLIB
    p->success(msg, raylib::GetTime());
#endif
  }
}

/// Show a warning notification
inline void warning(const std::string& msg) {
  if (auto* p = get_provider()) {
#ifdef AFTER_HOURS_USE_RAYLIB
    p->warning(msg, raylib::GetTime());
#endif
  }
}

/// Show an error notification
inline void error(const std::string& msg) {
  if (auto* p = get_provider()) {
#ifdef AFTER_HOURS_USE_RAYLIB
    p->error(msg, raylib::GetTime());
#endif
  }
}

} // namespace notifications

//-----------------------------------------------------------------------------
// Setup helpers
//-----------------------------------------------------------------------------

/// Add notifications singleton to an entity
inline void add_notifications_component(Entity& entity) {
  entity.addComponent<ProvidesNotifications>();
  EntityHelper::registerSingleton<ProvidesNotifications>(entity);
}

/// Register the cleanup system
inline void register_notification_systems(SystemManager& sm) {
  sm.register_update_system(std::make_unique<NotificationCleanupSystem>());
}

//-----------------------------------------------------------------------------
// Example usage:
//-----------------------------------------------------------------------------
/*
// Setup (in your game init):
add_notifications_component(singleton_entity);
register_notification_systems(system_manager);

// Show notifications anywhere in your code:
notifications::success("Game saved!");
notifications::error("Failed to load level");
notifications::info("Press F1 for help");

// In your render system, draw visible notifications:
if (auto* provider = notifications::get_provider()) {
  double time = GetTime();
  int y = 10;
  for (const auto* n : provider->get_visible(time)) {
    Color c = (n->level == NotificationLevel::Error) ? RED : WHITE;
    DrawText(n->message.c_str(), 10, y, 20, c);
    y += 25;
  }
}
*/

} // namespace afterhours

