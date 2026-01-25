#include <cassert>
#include <cstring>
#include <iostream>

// Include the memory arena directly for standalone usage
#include "../../../src/memory/arena.h"

// For this standalone example, we define the necessary types
// that would normally come from raylib/developer.h
namespace afterhours {
using RectangleType = struct { float x, y, width, height; };
using Vector2Type = struct { float x, y; };
using TextureType = struct { int id, width, height; };
using Color = struct { unsigned char r, g, b, a; };
}  // namespace afterhours

using namespace afterhours;

// Minimal type definitions needed for RenderPrimitive
enum class TextAlignment { Left, Center, Right, None };

struct TextStroke {
  float thickness = 0.f;
  Color color = {0, 0, 0, 255};
  bool has_stroke() const { return thickness > 0.f; }
};

struct TextShadow {
  float offset_x = 0.f;
  float offset_y = 0.f;
  Color color = {0, 0, 0, 128};
  bool has_shadow() const { return offset_x != 0.f || offset_y != 0.f; }
};

// Include bitset for corner settings
#include <bitset>
#include <optional>

// Define EntityID
using EntityID = int;

// ============================================================================
// Render Command Batching System (standalone version for testing)
// ============================================================================

enum class RenderPrimitiveType {
  Rectangle,
  RoundedRectangle,
  RectangleOutline,
  RoundedRectangleOutline,
  Text,
  Image,
  ScissorStart,
  ScissorEnd,
  Ring,
  RingSegment,
  NineSlice
};

const char* primitive_type_name(RenderPrimitiveType type) {
  switch (type) {
    case RenderPrimitiveType::Rectangle: return "Rectangle";
    case RenderPrimitiveType::RoundedRectangle: return "RoundedRectangle";
    case RenderPrimitiveType::RectangleOutline: return "RectangleOutline";
    case RenderPrimitiveType::RoundedRectangleOutline: return "RoundedRectangleOutline";
    case RenderPrimitiveType::Text: return "Text";
    case RenderPrimitiveType::Image: return "Image";
    case RenderPrimitiveType::ScissorStart: return "ScissorStart";
    case RenderPrimitiveType::ScissorEnd: return "ScissorEnd";
    case RenderPrimitiveType::Ring: return "Ring";
    case RenderPrimitiveType::RingSegment: return "RingSegment";
    case RenderPrimitiveType::NineSlice: return "NineSlice";
    default: return "Unknown";
  }
}

// Render primitive struct with union for different primitive data
struct RenderPrimitive {
  RenderPrimitiveType type;
  int layer = 0;
  EntityID entity_id = -1;

  union PrimitiveData {
    struct {
      RectangleType rect;
      Color fill_color;
      float roundness;
      int segments;
      std::bitset<4> corners;
    } rectangle;

    struct {
      RectangleType rect;
      Color color;
      float roundness;
      int segments;
      std::bitset<4> corners;
    } outline;

    struct {
      RectangleType rect;
      const char* text;
      const char* font_name;
      float font_size;
      Color color;
      TextAlignment alignment;
      bool has_stroke;
      float stroke_thickness;
      Color stroke_color;
      bool has_shadow;
      float shadow_offset_x;
      float shadow_offset_y;
      Color shadow_color;
    } text;

    struct {
      RectangleType dest_rect;
      RectangleType source_rect;
      TextureType texture;
      Color tint;
    } image;

    struct {
      int x;
      int y;
      int width;
      int height;
    } scissor;

    struct {
      float center_x;
      float center_y;
      float inner_radius;
      float outer_radius;
      int segments;
      Color color;
    } ring;

    struct {
      float center_x;
      float center_y;
      float inner_radius;
      float outer_radius;
      float start_angle;
      float end_angle;
      int segments;
      Color color;
    } ring_segment;

    struct {
      RectangleType rect;
      TextureType texture;
      int left;
      int top;
      int right;
      int bottom;
      Color tint;
    } nine_slice;

    PrimitiveData() { std::memset(this, 0, sizeof(PrimitiveData)); }
  } data;

  RenderPrimitive() : type(RenderPrimitiveType::Rectangle), layer(0), entity_id(-1), data() {}
};

static_assert(std::is_trivially_destructible_v<RenderPrimitive>,
              "RenderPrimitive must be trivially destructible for ArenaVector");

// Render command buffer using arena allocation
class RenderCommandBuffer {
private:
  ArenaVector<RenderPrimitive> commands_;
  Arena* arena_;

public:
  explicit RenderCommandBuffer(Arena& arena, size_t initial_capacity = 512)
    : commands_(arena, initial_capacity), arena_(&arena) {}

  void add_rectangle(const RectangleType& rect, Color fill, int layer,
                     EntityID entity_id = -1) {
    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::Rectangle;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
    cmd.data.rectangle.rect = rect;
    cmd.data.rectangle.fill_color = fill;
    cmd.data.rectangle.roundness = 0.0f;
    cmd.data.rectangle.segments = 0;
    cmd.data.rectangle.corners.reset();
  }

  void add_rounded_rectangle(const RectangleType& rect, Color fill,
                             float roundness, int segments,
                             const std::bitset<4>& corners, int layer,
                             EntityID entity_id = -1) {
    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::RoundedRectangle;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
    cmd.data.rectangle.rect = rect;
    cmd.data.rectangle.fill_color = fill;
    cmd.data.rectangle.roundness = roundness;
    cmd.data.rectangle.segments = segments;
    cmd.data.rectangle.corners = corners;
  }

  void add_text(const RectangleType& rect, const std::string& text,
                const std::string& font_name, float font_size, Color color,
                TextAlignment alignment, int layer, EntityID entity_id = -1) {
    char* text_copy = arena_->create_array_uninitialized<char>(text.size() + 1);
    if (text_copy) {
      std::copy(text.begin(), text.end(), text_copy);
      text_copy[text.size()] = '\0';
    }

    char* font_copy = arena_->create_array_uninitialized<char>(font_name.size() + 1);
    if (font_copy) {
      std::copy(font_name.begin(), font_name.end(), font_copy);
      font_copy[font_name.size()] = '\0';
    }

    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::Text;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
    cmd.data.text.rect = rect;
    cmd.data.text.text = text_copy;
    cmd.data.text.font_name = font_copy;
    cmd.data.text.font_size = font_size;
    cmd.data.text.color = color;
    cmd.data.text.alignment = alignment;
    cmd.data.text.has_stroke = false;
    cmd.data.text.has_shadow = false;
  }

  void add_scissor_start(int x, int y, int width, int height, int layer,
                         EntityID entity_id = -1) {
    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::ScissorStart;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
    cmd.data.scissor.x = x;
    cmd.data.scissor.y = y;
    cmd.data.scissor.width = width;
    cmd.data.scissor.height = height;
  }

  void add_scissor_end(int layer, EntityID entity_id = -1) {
    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::ScissorEnd;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
  }

  void add_ring(float center_x, float center_y, float inner_radius,
                float outer_radius, int segments, Color color, int layer,
                EntityID entity_id = -1) {
    auto& cmd = commands_.emplace_back();
    cmd.type = RenderPrimitiveType::Ring;
    cmd.layer = layer;
    cmd.entity_id = entity_id;
    cmd.data.ring.center_x = center_x;
    cmd.data.ring.center_y = center_y;
    cmd.data.ring.inner_radius = inner_radius;
    cmd.data.ring.outer_radius = outer_radius;
    cmd.data.ring.segments = segments;
    cmd.data.ring.color = color;
  }

  // Sort commands by layer and type for optimal batching
  void sort() {
    for (size_t i = 1; i < commands_.size(); ++i) {
      RenderPrimitive key = commands_[i];
      size_t j = i;
      while (j > 0 && (commands_[j - 1].layer > key.layer ||
                       (commands_[j - 1].layer == key.layer &&
                        static_cast<int>(commands_[j - 1].type) > static_cast<int>(key.type)))) {
        commands_[j] = commands_[j - 1];
        --j;
      }
      commands_[j] = key;
    }
  }

  [[nodiscard]] const ArenaVector<RenderPrimitive>& commands() const {
    return commands_;
  }

  [[nodiscard]] size_t size() const { return commands_.size(); }
  [[nodiscard]] bool empty() const { return commands_.empty(); }
  void clear() { commands_.clear(); }
};

// Stats collector for batch analysis
struct BatchStats {
  size_t total_commands = 0;
  size_t rectangle_count = 0;
  size_t rounded_rectangle_count = 0;
  size_t text_count = 0;
  size_t scissor_count = 0;
  size_t ring_count = 0;
  size_t potential_batches = 0;

  void analyze(const RenderCommandBuffer& buffer) {
    const auto& commands = buffer.commands();
    total_commands = commands.size();

    if (commands.empty()) return;

    RenderPrimitiveType current_type = commands[0].type;
    int current_layer = commands[0].layer;
    potential_batches = 1;

    for (size_t i = 0; i < commands.size(); ++i) {
      const auto& cmd = commands[i];

      // Count by type
      switch (cmd.type) {
        case RenderPrimitiveType::Rectangle:
          rectangle_count++;
          break;
        case RenderPrimitiveType::RoundedRectangle:
          rounded_rectangle_count++;
          break;
        case RenderPrimitiveType::Text:
          text_count++;
          break;
        case RenderPrimitiveType::ScissorStart:
        case RenderPrimitiveType::ScissorEnd:
          scissor_count++;
          break;
        case RenderPrimitiveType::Ring:
        case RenderPrimitiveType::RingSegment:
          ring_count++;
          break;
        default:
          break;
      }

      // Count batch transitions
      if (i > 0 && (cmd.type != current_type || cmd.layer != current_layer)) {
        potential_batches++;
        current_type = cmd.type;
        current_layer = cmd.layer;
      }
    }
  }

  void print() const {
    std::cout << "Batch Statistics:" << std::endl;
    std::cout << "  Total commands: " << total_commands << std::endl;
    std::cout << "  Rectangle commands: " << rectangle_count << std::endl;
    std::cout << "  Rounded rectangle commands: " << rounded_rectangle_count << std::endl;
    std::cout << "  Text commands: " << text_count << std::endl;
    std::cout << "  Scissor commands: " << scissor_count << std::endl;
    std::cout << "  Ring commands: " << ring_count << std::endl;
    std::cout << "  Potential batches: " << potential_batches << std::endl;
    if (total_commands > 0) {
      float batch_efficiency = 1.0f - (static_cast<float>(potential_batches) /
                                       static_cast<float>(total_commands));
      std::cout << "  Batch efficiency: " << (batch_efficiency * 100.0f) << "%" << std::endl;
    }
  }
};

int main() {
  std::cout << "=== Render Command Batching Example ===" << std::endl;
  std::cout << "This example demonstrates how render commands are collected" << std::endl;
  std::cout << "into a buffer and sorted for optimal batching.\n" << std::endl;

  // Test 1: Create arena and command buffer
  std::cout << "1. Creating Arena and RenderCommandBuffer:" << std::endl;
  Arena arena(1024 * 1024);  // 1MB arena
  RenderCommandBuffer buffer(arena, 64);
  std::cout << "   Arena created with 1MB capacity" << std::endl;
  std::cout << "   RenderCommandBuffer created with 64 initial capacity" << std::endl;
  assert(buffer.empty());
  std::cout << "   Buffer is initially empty: PASS" << std::endl;

  // Test 2: Add commands in non-optimal order (different layers interleaved)
  std::cout << "\n2. Adding render commands (in non-optimal order):" << std::endl;

  // Add commands with varying layers (not sorted)
  buffer.add_rectangle({10, 10, 100, 50}, {255, 0, 0, 255}, 2, 1);
  std::cout << "   Added Rectangle at layer 2, entity 1" << std::endl;

  buffer.add_text({20, 20, 80, 30}, "Hello World", "default", 16.0f,
                  {255, 255, 255, 255}, TextAlignment::Center, 3, 2);
  std::cout << "   Added Text at layer 3, entity 2" << std::endl;

  buffer.add_rectangle({50, 50, 100, 50}, {0, 255, 0, 255}, 1, 3);
  std::cout << "   Added Rectangle at layer 1, entity 3" << std::endl;

  buffer.add_rounded_rectangle({100, 100, 80, 40}, {0, 0, 255, 255},
                                0.5f, 8, std::bitset<4>("1111"), 2, 4);
  std::cout << "   Added RoundedRectangle at layer 2, entity 4" << std::endl;

  buffer.add_rectangle({150, 150, 60, 60}, {255, 255, 0, 255}, 1, 5);
  std::cout << "   Added Rectangle at layer 1, entity 5" << std::endl;

  buffer.add_scissor_start(0, 0, 800, 600, 0, 6);
  std::cout << "   Added ScissorStart at layer 0, entity 6" << std::endl;

  buffer.add_text({200, 200, 100, 30}, "Button", "bold", 14.0f,
                  {0, 0, 0, 255}, TextAlignment::Left, 2, 7);
  std::cout << "   Added Text at layer 2, entity 7" << std::endl;

  buffer.add_ring(300, 300, 20, 30, 32, {128, 128, 255, 255}, 2, 8);
  std::cout << "   Added Ring at layer 2, entity 8" << std::endl;

  buffer.add_scissor_end(0, 9);
  std::cout << "   Added ScissorEnd at layer 0, entity 9" << std::endl;

  assert(buffer.size() == 9);
  std::cout << "   Buffer size: " << buffer.size() << " commands" << std::endl;

  // Test 3: Analyze before sorting
  std::cout << "\n3. Analyzing commands BEFORE sorting:" << std::endl;
  BatchStats stats_before;
  stats_before.analyze(buffer);
  stats_before.print();

  std::cout << "\n   Command order before sort:" << std::endl;
  const auto& cmds_before = buffer.commands();
  for (size_t i = 0; i < cmds_before.size(); ++i) {
    std::cout << "   [" << i << "] Layer " << cmds_before[i].layer
              << " " << primitive_type_name(cmds_before[i].type)
              << " (entity " << cmds_before[i].entity_id << ")" << std::endl;
  }

  // Test 4: Sort commands
  std::cout << "\n4. Sorting commands by layer and type:" << std::endl;
  buffer.sort();
  std::cout << "   Sort complete!" << std::endl;

  // Test 5: Analyze after sorting
  std::cout << "\n5. Analyzing commands AFTER sorting:" << std::endl;
  BatchStats stats_after;
  stats_after.analyze(buffer);
  stats_after.print();

  std::cout << "\n   Command order after sort:" << std::endl;
  const auto& cmds_after = buffer.commands();
  for (size_t i = 0; i < cmds_after.size(); ++i) {
    std::cout << "   [" << i << "] Layer " << cmds_after[i].layer
              << " " << primitive_type_name(cmds_after[i].type)
              << " (entity " << cmds_after[i].entity_id << ")" << std::endl;
  }

  // Verify sort order
  bool sorted_correctly = true;
  for (size_t i = 1; i < cmds_after.size(); ++i) {
    bool layer_ok = cmds_after[i].layer >= cmds_after[i-1].layer;
    bool type_ok = cmds_after[i].layer != cmds_after[i-1].layer ||
                   static_cast<int>(cmds_after[i].type) >=
                   static_cast<int>(cmds_after[i-1].type);
    if (!layer_ok || !type_ok) {
      sorted_correctly = false;
      break;
    }
  }
  assert(sorted_correctly);
  std::cout << "\n   Sort verification: PASS" << std::endl;

  // Test 6: Check arena usage
  std::cout << "\n6. Arena memory statistics:" << std::endl;
  std::cout << "   Arena used: " << arena.used() << " bytes" << std::endl;
  std::cout << "   Arena capacity: " << arena.capacity() << " bytes" << std::endl;
  std::cout << "   Arena usage: " << arena.usage_percent() << "%" << std::endl;
  std::cout << "   Allocation count: " << arena.allocation_count() << std::endl;

  // Test 7: Clear and reset
  std::cout << "\n7. Clear and reset (simulating frame end):" << std::endl;
  buffer.clear();
  assert(buffer.empty());
  std::cout << "   Buffer cleared" << std::endl;

  arena.reset();
  std::cout << "   Arena reset" << std::endl;
  std::cout << "   Arena used after reset: " << arena.used() << " bytes" << std::endl;

  // Test 8: Demonstrate typical frame usage pattern
  std::cout << "\n8. Typical frame usage pattern:" << std::endl;

  // Simulate multiple frames
  for (int frame = 0; frame < 3; ++frame) {
    std::cout << "   Frame " << frame << ":" << std::endl;

    // Reset arena at frame start
    arena.reset();

    // Create command buffer for this frame
    RenderCommandBuffer frame_buffer(arena, 32);

    // Add some commands
    frame_buffer.add_rectangle({0, 0, 800, 600}, {30, 30, 30, 255}, 0);  // Background
    frame_buffer.add_rectangle({100, 100, 200, 100}, {50, 50, 150, 255}, 1);  // Panel
    frame_buffer.add_text({110, 110, 180, 30}, "Frame " + std::to_string(frame),
                          "default", 20.0f, {255, 255, 255, 255},
                          TextAlignment::Center, 2);

    // Sort and analyze
    frame_buffer.sort();

    BatchStats frame_stats;
    frame_stats.analyze(frame_buffer);

    std::cout << "     Commands: " << frame_stats.total_commands
              << ", Batches: " << frame_stats.potential_batches
              << ", Arena used: " << arena.used() << " bytes" << std::endl;
  }

  std::cout << "\n=== All Render Command Batching tests passed! ===" << std::endl;
  std::cout << "\nKey benefits of command batching:" << std::endl;
  std::cout << "  - Zero allocations per frame (arena-backed)" << std::endl;
  std::cout << "  - Sorting enables batching of similar commands" << std::endl;
  std::cout << "  - Reduced draw call overhead" << std::endl;
  std::cout << "  - Layer-based rendering order" << std::endl;

  return 0;
}
