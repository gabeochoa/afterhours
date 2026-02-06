#pragma once

#include <bitset>
#include <cstring>
#include <optional>

#include "../../memory/arena.h"
#include "components.h"

namespace afterhours {
namespace ui {

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

// Render primitive struct with union for different primitive data
struct RenderPrimitive {
  RenderPrimitiveType type;
  int layer = 0;
  EntityID entity_id = -1;  // For debugging

  // Union of all primitive data types
  union PrimitiveData {
    struct {
      RectangleType rect;
      Color fill_color;
      float roundness;
      int segments;
      std::bitset<4> corners;
      float rotation;  // Rotation in degrees (clockwise, around center)
    } rectangle;

    struct {
      RectangleType rect;
      Color color;
      float roundness;
      int segments;
      std::bitset<4> corners;
      float thickness;  // Line thickness for outline (0 = default thin line)
    } outline;

    struct {
      RectangleType rect;
      const char* text;  // Points into arena memory
      const char* font_name;
      float font_size;
      Color color;
      TextAlignment alignment;
      // Optional stroke/shadow data
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
      int left, top, right, bottom;
      Color tint;
    } nine_slice;

    // Default constructor for union
    PrimitiveData() { std::memset(this, 0, sizeof(PrimitiveData)); }
  } data;

  RenderPrimitive() : type(RenderPrimitiveType::Rectangle), layer(0), entity_id(-1), data() {}

private:
  // Helper constructor for factory methods
  RenderPrimitive(RenderPrimitiveType t, int l, EntityID eid)
    : type(t), layer(l), entity_id(eid), data() {}

public:
  // Factory methods for cleaner construction
  static RenderPrimitive rectangle(const RectangleType& rect, Color fill,
                                   int layer, EntityID entity_id = -1,
                                   float rotation = 0.0f) {
    RenderPrimitive cmd(RenderPrimitiveType::Rectangle, layer, entity_id);
    cmd.data.rectangle.rect = rect;
    cmd.data.rectangle.fill_color = fill;
    cmd.data.rectangle.roundness = 0.0f;
    cmd.data.rectangle.segments = 0;
    cmd.data.rectangle.corners.reset();
    cmd.data.rectangle.rotation = rotation;
    return cmd;
  }

  static RenderPrimitive rounded_rectangle(const RectangleType& rect, Color fill,
                                           float roundness, int segments,
                                           const std::bitset<4>& corners,
                                           int layer, EntityID entity_id = -1,
                                           float rotation = 0.0f) {
    RenderPrimitive cmd(RenderPrimitiveType::RoundedRectangle, layer, entity_id);
    cmd.data.rectangle.rect = rect;
    cmd.data.rectangle.fill_color = fill;
    cmd.data.rectangle.roundness = roundness;
    cmd.data.rectangle.segments = segments;
    cmd.data.rectangle.corners = corners;
    cmd.data.rectangle.rotation = rotation;
    return cmd;
  }

  static RenderPrimitive rectangle_outline(const RectangleType& rect, Color color,
                                           int layer, EntityID entity_id = -1,
                                           float thickness = 0.0f) {
    RenderPrimitive cmd(RenderPrimitiveType::RectangleOutline, layer, entity_id);
    cmd.data.outline.rect = rect;
    cmd.data.outline.color = color;
    cmd.data.outline.roundness = 0.0f;
    cmd.data.outline.segments = 0;
    cmd.data.outline.corners.reset();
    cmd.data.outline.thickness = thickness;
    return cmd;
  }

  static RenderPrimitive rounded_rectangle_outline(const RectangleType& rect, Color color,
                                                   float roundness, int segments,
                                                   const std::bitset<4>& corners,
                                                   int layer, EntityID entity_id = -1,
                                                   float thickness = 0.0f) {
    RenderPrimitive cmd(RenderPrimitiveType::RoundedRectangleOutline, layer, entity_id);
    cmd.data.outline.rect = rect;
    cmd.data.outline.color = color;
    cmd.data.outline.roundness = roundness;
    cmd.data.outline.segments = segments;
    cmd.data.outline.corners = corners;
    cmd.data.outline.thickness = thickness;
    return cmd;
  }

  static RenderPrimitive image(const RectangleType& dest_rect,
                               const RectangleType& source_rect,
                               TextureType texture, Color tint,
                               int layer, EntityID entity_id = -1) {
    RenderPrimitive cmd(RenderPrimitiveType::Image, layer, entity_id);
    cmd.data.image.dest_rect = dest_rect;
    cmd.data.image.source_rect = source_rect;
    cmd.data.image.texture = texture;
    cmd.data.image.tint = tint;
    return cmd;
  }

  static RenderPrimitive scissor_start(int x, int y, int width, int height,
                                       int layer, EntityID entity_id = -1) {
    RenderPrimitive cmd(RenderPrimitiveType::ScissorStart, layer, entity_id);
    cmd.data.scissor.x = x;
    cmd.data.scissor.y = y;
    cmd.data.scissor.width = width;
    cmd.data.scissor.height = height;
    return cmd;
  }

  static RenderPrimitive scissor_end(int layer, EntityID entity_id = -1) {
    return RenderPrimitive(RenderPrimitiveType::ScissorEnd, layer, entity_id);
  }

  static RenderPrimitive ring(float center_x, float center_y,
                              float inner_radius, float outer_radius,
                              int segments, Color color,
                              int layer, EntityID entity_id = -1) {
    RenderPrimitive cmd(RenderPrimitiveType::Ring, layer, entity_id);
    cmd.data.ring.center_x = center_x;
    cmd.data.ring.center_y = center_y;
    cmd.data.ring.inner_radius = inner_radius;
    cmd.data.ring.outer_radius = outer_radius;
    cmd.data.ring.segments = segments;
    cmd.data.ring.color = color;
    return cmd;
  }

  static RenderPrimitive ring_segment(float center_x, float center_y,
                                      float inner_radius, float outer_radius,
                                      float start_angle, float end_angle,
                                      int segments, Color color,
                                      int layer, EntityID entity_id = -1) {
    RenderPrimitive cmd(RenderPrimitiveType::RingSegment, layer, entity_id);
    cmd.data.ring_segment.center_x = center_x;
    cmd.data.ring_segment.center_y = center_y;
    cmd.data.ring_segment.inner_radius = inner_radius;
    cmd.data.ring_segment.outer_radius = outer_radius;
    cmd.data.ring_segment.start_angle = start_angle;
    cmd.data.ring_segment.end_angle = end_angle;
    cmd.data.ring_segment.segments = segments;
    cmd.data.ring_segment.color = color;
    return cmd;
  }

  static RenderPrimitive nine_slice(const RectangleType& rect, TextureType texture,
                                    int left, int top, int right, int bottom,
                                    Color tint, int layer, EntityID entity_id = -1) {
    RenderPrimitive cmd(RenderPrimitiveType::NineSlice, layer, entity_id);
    cmd.data.nine_slice.rect = rect;
    cmd.data.nine_slice.texture = texture;
    cmd.data.nine_slice.left = left;
    cmd.data.nine_slice.top = top;
    cmd.data.nine_slice.right = right;
    cmd.data.nine_slice.bottom = bottom;
    cmd.data.nine_slice.tint = tint;
    return cmd;
  }
};

// Static assertion to ensure RenderPrimitive is trivially destructible for ArenaVector
static_assert(std::is_trivially_destructible_v<RenderPrimitive>,
              "RenderPrimitive must be trivially destructible for ArenaVector");

// Render command buffer using arena allocation for zero-allocation rendering
class RenderCommandBuffer {
private:
  ArenaVector<RenderPrimitive> commands_;
  Arena* arena_;

public:
  explicit RenderCommandBuffer(Arena& arena, size_t initial_capacity = 512)
    : commands_(arena, initial_capacity), arena_(&arena) {}

  // Add a filled rectangle
  void add_rectangle(const RectangleType& rect, Color fill, int layer,
                     EntityID entity_id = -1, float rotation = 0.0f) {
    commands_.push_back(RenderPrimitive::rectangle(rect, fill, layer, entity_id, rotation));
  }

  // Add a rounded rectangle
  void add_rounded_rectangle(const RectangleType& rect, Color fill,
                             float roundness, int segments,
                             const std::bitset<4>& corners, int layer,
                             EntityID entity_id = -1, float rotation = 0.0f) {
    commands_.push_back(RenderPrimitive::rounded_rectangle(
        rect, fill, roundness, segments, corners, layer, entity_id, rotation));
  }

  // Add rectangle outline
  void add_rectangle_outline(const RectangleType& rect, Color color, int layer,
                             EntityID entity_id = -1, float thickness = 0.0f) {
    commands_.push_back(RenderPrimitive::rectangle_outline(rect, color, layer, entity_id, thickness));
  }

  // Add rounded rectangle outline
  void add_rounded_rectangle_outline(const RectangleType& rect, Color color,
                                     float roundness, int segments,
                                     const std::bitset<4>& corners, int layer,
                                     EntityID entity_id = -1, float thickness = 0.0f) {
    commands_.push_back(RenderPrimitive::rounded_rectangle_outline(
        rect, color, roundness, segments, corners, layer, entity_id, thickness));
  }

  // Add text with optional stroke and shadow
  void add_text(const RectangleType& rect, const std::string& text,
                const std::string& font_name, float font_size, Color color,
                TextAlignment alignment, int layer, EntityID entity_id = -1,
                const std::optional<TextStroke>& stroke = std::nullopt,
                const std::optional<TextShadow>& shadow = std::nullopt) {
    // Copy strings to arena for stable pointers
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

    // Stroke data
    cmd.data.text.has_stroke = stroke.has_value() && stroke->has_stroke();
    if (cmd.data.text.has_stroke) {
      cmd.data.text.stroke_thickness = stroke->thickness;
      cmd.data.text.stroke_color = stroke->color;
    }

    // Shadow data
    cmd.data.text.has_shadow = shadow.has_value() && shadow->has_shadow();
    if (cmd.data.text.has_shadow) {
      cmd.data.text.shadow_offset_x = shadow->offset_x;
      cmd.data.text.shadow_offset_y = shadow->offset_y;
      cmd.data.text.shadow_color = shadow->color;
    }
  }

  // Add image
  void add_image(const RectangleType& dest_rect, const RectangleType& source_rect,
                 TextureType texture, Color tint, int layer,
                 EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::image(dest_rect, source_rect, texture, tint, layer, entity_id));
  }

  // Add scissor start
  void add_scissor_start(int x, int y, int width, int height, int layer,
                         EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::scissor_start(x, y, width, height, layer, entity_id));
  }

  // Add scissor end
  void add_scissor_end(int layer, EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::scissor_end(layer, entity_id));
  }

  // Add ring (for circular progress background)
  void add_ring(float center_x, float center_y, float inner_radius,
                float outer_radius, int segments, Color color, int layer,
                EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::ring(center_x, center_y, inner_radius,
                                              outer_radius, segments, color, layer, entity_id));
  }

  // Add ring segment (for circular progress fill)
  void add_ring_segment(float center_x, float center_y, float inner_radius,
                        float outer_radius, float start_angle, float end_angle,
                        int segments, Color color, int layer,
                        EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::ring_segment(center_x, center_y, inner_radius,
                                                      outer_radius, start_angle, end_angle,
                                                      segments, color, layer, entity_id));
  }

  // Add nine-slice border
  void add_nine_slice(const RectangleType& rect, TextureType texture,
                      int left, int top, int right, int bottom, Color tint,
                      int layer, EntityID entity_id = -1) {
    commands_.push_back(RenderPrimitive::nine_slice(rect, texture, left, top, right, bottom,
                                                    tint, layer, entity_id));
  }

  // Sort commands by layer and type for optimal batching
  void sort() {
    // Simple insertion sort for arena-backed vector (stable for same layer/type)
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

// Batched renderer that executes render commands
class BatchedRenderer {
public:
  // Statistics for profiling
  struct Stats {
    size_t total_commands = 0;
    size_t rectangle_batches = 0;
    size_t text_commands = 0;
    size_t image_commands = 0;
    size_t scissor_operations = 0;
  };

  void render(const RenderCommandBuffer& buffer, FontManager& fonts) {
    stats_ = {};  // Reset stats
    const auto& commands = buffer.commands();
    stats_.total_commands = commands.size();

    size_t i = 0;
    while (i < commands.size()) {
      const auto& cmd = commands[i];

      switch (cmd.type) {
        case RenderPrimitiveType::Rectangle: {
          size_t batch_end = find_batch_end(commands, i);
          render_rectangle_batch(commands, i, batch_end);
          stats_.rectangle_batches++;
          i = batch_end;
          break;
        }

        case RenderPrimitiveType::RoundedRectangle: {
          size_t batch_end = find_batch_end(commands, i);
          render_rounded_rectangle_batch(commands, i, batch_end);
          stats_.rectangle_batches++;
          i = batch_end;
          break;
        }

        case RenderPrimitiveType::RectangleOutline: {
          size_t batch_end = find_batch_end(commands, i);
          render_outline_batch(commands, i, batch_end);
          i = batch_end;
          break;
        }

        case RenderPrimitiveType::RoundedRectangleOutline: {
          size_t batch_end = find_batch_end(commands, i);
          render_rounded_outline_batch(commands, i, batch_end);
          i = batch_end;
          break;
        }

        case RenderPrimitiveType::Text:
          render_text(cmd, fonts);
          stats_.text_commands++;
          i++;
          break;

        case RenderPrimitiveType::Image:
          render_image(cmd);
          stats_.image_commands++;
          i++;
          break;

        case RenderPrimitiveType::ScissorStart:
          begin_scissor_mode(cmd.data.scissor.x, cmd.data.scissor.y,
                            cmd.data.scissor.width, cmd.data.scissor.height);
          stats_.scissor_operations++;
          i++;
          break;

        case RenderPrimitiveType::ScissorEnd:
          end_scissor_mode();
          stats_.scissor_operations++;
          i++;
          break;

        case RenderPrimitiveType::Ring:
          render_ring(cmd);
          i++;
          break;

        case RenderPrimitiveType::RingSegment:
          render_ring_segment(cmd);
          i++;
          break;

        case RenderPrimitiveType::NineSlice:
          render_nine_slice(cmd);
          i++;
          break;

        default:
          i++;
          break;
      }
    }
  }

  [[nodiscard]] const Stats& stats() const { return stats_; }

private:
  Stats stats_;

  // Find the end of a batch of commands with same type and layer
  static size_t find_batch_end(const ArenaVector<RenderPrimitive>& cmds, size_t start) {
    if (start >= cmds.size()) return start;
    const auto& first = cmds[start];
    size_t end = start + 1;
    while (end < cmds.size() &&
           cmds[end].type == first.type &&
           cmds[end].layer == first.layer) {
      end++;
    }
    return end;
  }

  void render_rectangle_batch(const ArenaVector<RenderPrimitive>& cmds,
                              size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      const auto& rect = cmds[i].data.rectangle;
      draw_rectangle_rounded_rotated(rect.rect, 0.0f, 0, rect.fill_color,
                                     std::bitset<4>().reset(), rect.rotation);
    }
  }

  void render_rounded_rectangle_batch(const ArenaVector<RenderPrimitive>& cmds,
                                      size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      const auto& rect = cmds[i].data.rectangle;
      draw_rectangle_rounded_rotated(rect.rect, rect.roundness, rect.segments,
                                     rect.fill_color, rect.corners, rect.rotation);
    }
  }

  void render_outline_batch(const ArenaVector<RenderPrimitive>& cmds,
                            size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      const auto& outline = cmds[i].data.outline;
      if (outline.thickness > 1.0f) {
        // Draw multiple outlines for thickness effect
        for (float t = 0; t < outline.thickness; t += 1.0f) {
          RectangleType thickRect = {
            outline.rect.x - t,
            outline.rect.y - t,
            outline.rect.width + t * 2.0f,
            outline.rect.height + t * 2.0f
          };
          draw_rectangle_outline(thickRect, outline.color);
        }
      } else {
        draw_rectangle_outline(outline.rect, outline.color);
      }
    }
  }

  void render_rounded_outline_batch(const ArenaVector<RenderPrimitive>& cmds,
                                    size_t start, size_t end) {
    for (size_t i = start; i < end; ++i) {
      const auto& outline = cmds[i].data.outline;
      if (outline.thickness > 1.0f) {
        // Draw multiple outlines for thickness effect
        for (float t = 0; t < outline.thickness; t += 1.0f) {
          RectangleType thickRect = {
            outline.rect.x - t,
            outline.rect.y - t,
            outline.rect.width + t * 2.0f,
            outline.rect.height + t * 2.0f
          };
          draw_rectangle_rounded_lines(thickRect, outline.roundness,
                                       outline.segments, outline.color, outline.corners);
        }
      } else {
        draw_rectangle_rounded_lines(outline.rect, outline.roundness,
                                     outline.segments, outline.color, outline.corners);
      }
    }
  }

  void render_text(const RenderPrimitive& cmd, FontManager& fonts) {
    if (!cmd.data.text.text) return;

    // Set font if specified
    if (cmd.data.text.font_name && cmd.data.text.font_name[0] != '\0') {
      fonts.set_active(cmd.data.text.font_name);
    }

    Font font = fonts.get_active_font();
    float fontSize = cmd.data.text.font_size;
    float spacing = 1.0f;

    Vector2Type startPos = {cmd.data.text.rect.x, cmd.data.text.rect.y};

    // Handle alignment
    if (cmd.data.text.alignment == TextAlignment::Center) {
      Vector2Type textSize = measure_text_utf8(font, cmd.data.text.text, fontSize, spacing);
      // Calculate centered position, clamping to prevent text starting before container left edge
      float centered_x = cmd.data.text.rect.x + (cmd.data.text.rect.width - textSize.x) / 2.0f;
      startPos.x = std::max(cmd.data.text.rect.x, centered_x);
      startPos.y = cmd.data.text.rect.y + (cmd.data.text.rect.height - textSize.y) / 2.0f;
    } else if (cmd.data.text.alignment == TextAlignment::Right) {
      Vector2Type textSize = measure_text_utf8(font, cmd.data.text.text, fontSize, spacing);
      // Clamp right-aligned text to not start before container left edge
      float right_x = cmd.data.text.rect.x + cmd.data.text.rect.width - textSize.x;
      startPos.x = std::max(cmd.data.text.rect.x, right_x);
    }

    // Draw shadow first
    if (cmd.data.text.has_shadow) {
      Vector2Type shadowPos = {startPos.x + cmd.data.text.shadow_offset_x,
                               startPos.y + cmd.data.text.shadow_offset_y};
      draw_text_ex(font, cmd.data.text.text, shadowPos, fontSize, spacing,
                   cmd.data.text.shadow_color);
    }

    // Draw stroke (8 directions)
    if (cmd.data.text.has_stroke) {
      float t = cmd.data.text.stroke_thickness;
      const float offsets[][2] = {
        {-t, -t}, {0, -t}, {t, -t}, {-t, 0}, {t, 0}, {-t, t}, {0, t}, {t, t}
      };
      for (const auto& offset : offsets) {
        Vector2Type strokePos = {startPos.x + offset[0], startPos.y + offset[1]};
        draw_text_ex(font, cmd.data.text.text, strokePos, fontSize, spacing,
                     cmd.data.text.stroke_color);
      }
    }

    // Draw main text
    draw_text_ex(font, cmd.data.text.text, startPos, fontSize, spacing,
                 cmd.data.text.color);
  }

  void render_image(const RenderPrimitive& cmd) {
    texture_manager::draw_texture_pro(
      cmd.data.image.texture,
      cmd.data.image.source_rect,
      cmd.data.image.dest_rect,
      {cmd.data.image.dest_rect.width, cmd.data.image.dest_rect.height},
      0.f,
      cmd.data.image.tint
    );
  }

  void render_ring(const RenderPrimitive& cmd) {
    draw_ring(cmd.data.ring.center_x, cmd.data.ring.center_y,
              cmd.data.ring.inner_radius, cmd.data.ring.outer_radius,
              cmd.data.ring.segments, cmd.data.ring.color);
  }

  void render_ring_segment(const RenderPrimitive& cmd) {
    draw_ring_segment(cmd.data.ring_segment.center_x, cmd.data.ring_segment.center_y,
                      cmd.data.ring_segment.inner_radius, cmd.data.ring_segment.outer_radius,
                      cmd.data.ring_segment.start_angle, cmd.data.ring_segment.end_angle,
                      cmd.data.ring_segment.segments, cmd.data.ring_segment.color);
  }

  void render_nine_slice(const RenderPrimitive& cmd) {
    draw_texture_npatch(cmd.data.nine_slice.texture, cmd.data.nine_slice.rect,
                        cmd.data.nine_slice.left, cmd.data.nine_slice.top,
                        cmd.data.nine_slice.right, cmd.data.nine_slice.bottom,
                        cmd.data.nine_slice.tint);
  }
};

// Global render arena for command buffering
// Reset at frame start, after previous frame's render is complete
inline Arena& get_render_arena() {
  static Arena arena(2 * 1024 * 1024);  // 2MB for render commands
  return arena;
}

}  // namespace ui
}  // namespace afterhours
