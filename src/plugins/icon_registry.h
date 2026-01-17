// Icon Registry for Afterhours
// Centralized icon management with fallback symbols and mirrored pairs.
//
// Use cases:
// - Games: Inventory icons, skill icons, status effects, HUD elements
// - Level editors: Tool icons, object palette icons
// - Apps: Toolbar and menu icons
//
// To integrate into Afterhours:
// 1. Add as src/plugins/ui/icon_registry.h
// 2. Games define their own IconId enum or use string IDs

#pragma once

#include <optional>
#include <string>
#include <unordered_map>

namespace afterhours {
namespace ui {

//=============================================================================
// ICON INFO
//=============================================================================

/// Metadata for a registered icon
struct IconInfo {
  std::string id;             // Unique identifier
  std::string name;           // Human-readable name (for tooltips, debugging)
  std::string resource_path;  // Path to icon texture/sprite (empty = no visual)
  char fallback_symbol;       // Text character when icon unavailable
  bool is_mirrored;           // Draw flipped horizontally
  std::string mirror_of;      // If mirrored, the base icon ID
};

//=============================================================================
// ICON REGISTRY
//=============================================================================

/// Central registry for all icons in the application.
/// Icons are registered once and looked up by ID.
class IconRegistry {
public:
  /// Get the singleton instance
  static IconRegistry& instance() {
    static IconRegistry inst;
    return inst;
  }
  
  /// Register an icon
  void register_icon(const std::string& id, 
                     const std::string& name,
                     const std::string& resource_path = "",
                     char fallback_symbol = ' ',
                     bool is_mirrored = false,
                     const std::string& mirror_of = "") {
    icons_[id] = IconInfo{
      id, name, resource_path, fallback_symbol, is_mirrored, mirror_of
    };
  }
  
  /// Register a mirrored version of an existing icon
  void register_mirrored(const std::string& id,
                         const std::string& name,
                         const std::string& base_icon_id,
                         char fallback_symbol = ' ') {
    auto base = get(base_icon_id);
    std::string resource = base ? base->resource_path : "";
    icons_[id] = IconInfo{
      id, name, resource, fallback_symbol, true, base_icon_id
    };
  }
  
  /// Get icon info by ID (returns nullopt if not registered)
  std::optional<IconInfo> get(const std::string& id) const {
    auto it = icons_.find(id);
    if (it != icons_.end()) {
      return it->second;
    }
    return std::nullopt;
  }
  
  /// Check if an icon is registered
  bool has_icon(const std::string& id) const {
    return icons_.find(id) != icons_.end();
  }
  
  /// Get the fallback symbol for an icon (space if not found)
  char get_symbol(const std::string& id) const {
    auto info = get(id);
    return info ? info->fallback_symbol : ' ';
  }
  
  /// Check if icon has a visual resource (not just fallback)
  bool has_visual(const std::string& id) const {
    auto info = get(id);
    return info && !info->resource_path.empty();
  }
  
  /// Clear all registered icons
  void clear() {
    icons_.clear();
  }
  
  /// Get all registered icon IDs
  std::vector<std::string> get_all_ids() const {
    std::vector<std::string> ids;
    ids.reserve(icons_.size());
    for (const auto& [id, _] : icons_) {
      ids.push_back(id);
    }
    return ids;
  }

private:
  IconRegistry() = default;
  std::unordered_map<std::string, IconInfo> icons_;
};

/// Convenience function to get the global registry
inline IconRegistry& icons() {
  return IconRegistry::instance();
}

//=============================================================================
// COMMON ICON IDS (Optional - games can define their own)
//=============================================================================

namespace common_icons {

// File operations
constexpr const char* New = "file.new";
constexpr const char* Open = "file.open";
constexpr const char* Save = "file.save";
constexpr const char* SaveAs = "file.save_as";
constexpr const char* Print = "file.print";

// Edit operations
constexpr const char* Undo = "edit.undo";
constexpr const char* Redo = "edit.redo";
constexpr const char* Cut = "edit.cut";
constexpr const char* Copy = "edit.copy";
constexpr const char* Paste = "edit.paste";
constexpr const char* Delete = "edit.delete";
constexpr const char* SelectAll = "edit.select_all";
constexpr const char* Find = "edit.find";
constexpr const char* Replace = "edit.replace";

// Navigation
constexpr const char* ZoomIn = "view.zoom_in";
constexpr const char* ZoomOut = "view.zoom_out";
constexpr const char* ZoomReset = "view.zoom_reset";

// Help
constexpr const char* Help = "help";
constexpr const char* About = "about";

/// Register a standard set of icons with fallback symbols
inline void register_defaults() {
  auto& reg = icons();
  
  // File operations
  reg.register_icon(New, "New", "", '+');
  reg.register_icon(Open, "Open", "", 'O');
  reg.register_icon(Save, "Save", "", 'S');
  reg.register_icon(Print, "Print", "", 'P');
  
  // Edit operations - undo/redo are mirrored pair
  reg.register_icon(Undo, "Undo", "", '<');
  reg.register_mirrored(Redo, "Redo", Undo, '>');
  reg.register_icon(Cut, "Cut", "", 'X');
  reg.register_icon(Copy, "Copy", "", 'C');
  reg.register_icon(Paste, "Paste", "", 'V');
  
  // Navigation
  reg.register_icon(ZoomIn, "Zoom In", "", '+');
  reg.register_icon(ZoomOut, "Zoom Out", "", '-');
  
  // Help
  reg.register_icon(Help, "Help", "", '?');
}

} // namespace common_icons

} // namespace ui
} // namespace afterhours

//=============================================================================
// USAGE EXAMPLE
//=============================================================================
/*
#include "icon_registry.h"

int main() {
  using namespace afterhours::ui;
  
  // Option 1: Use common icons
  common_icons::register_defaults();
  
  // Option 2: Register custom icons for your game
  icons().register_icon("item.sword", "Iron Sword", "icons/sword.png", 's');
  icons().register_icon("item.potion", "Health Potion", "icons/potion.png", 'p');
  icons().register_icon("skill.fireball", "Fireball", "icons/fireball.png", 'F');
  
  // Mirrored icons (arrows, etc.)
  icons().register_icon("nav.left", "Previous", "icons/arrow.png", '<');
  icons().register_mirrored("nav.right", "Next", "nav.left", '>');
  
  // Drawing with icon
  void draw_button(const std::string& icon_id, const std::string& label) {
    auto icon = icons().get(icon_id);
    
    if (icon && !icon->resource_path.empty()) {
      // Draw actual icon texture
      Texture tex = LoadTexture(icon->resource_path.c_str());
      if (icon->is_mirrored) {
        // Flip horizontally
        DrawTextureRec(tex, {0, 0, -tex.width, tex.height}, {x, y}, WHITE);
      } else {
        DrawTexture(tex, x, y, WHITE);
      }
    } else if (icon) {
      // Draw fallback symbol
      DrawText(&icon->fallback_symbol, x, y, 16, BLACK);
    }
    
    // Always draw text label
    DrawText(label.c_str(), x + icon_width, y, 16, BLACK);
  }
  
  return 0;
}
*/

