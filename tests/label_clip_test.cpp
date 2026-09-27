// label_clip_test.cpp
// A label is scissored to its own box ("Clip: text is clipped at
// container boundary" is the documented default). Before this, nothing
// clipped a label to its own rect, so an over-long label -- the doubled
// string of a pseudo-locale stress, say -- drew straight over its
// neighbours. These tests inspect the batched renderer's command
// buffer: the label's text must sit between a scissor of its own box
// and a restore of whatever clip surrounded it.

#include "ui_test_harness.h"

using namespace afterhours;
using namespace afterhours::ui;
using namespace afterhours::ui::imm;
using ui_test::ImmTestHarness;
using ui_test::TestInputAction;

static ComponentConfig positioned(float x, float y, float w, float height) {
  return ComponentConfig{}.with_size({pixels(w), pixels(height)})
      .with_absolute_position(x, y).with_corner_radius(0)
      .with_skip_grid_snap(true).with_transparent_bg();
}

static std::vector<RenderPrimitive> collect(ImmTestHarness &h, Entity &ent) {
  auto &fonts = *h.render_font();
  Arena arena(Arena::DEFAULT_CAPACITY);
  RenderCommandBuffer buffer(arena);
  RenderBatched<TestInputAction> renderer;
  renderer.collect(buffer, h.context(), fonts, ent, 1000);
  const auto &cmds = buffer.commands();
  return std::vector<RenderPrimitive>(cmds.begin(), cmds.end());
}

static bool is_scissor(const RenderPrimitive &c, int x, int y, int w, int h) {
  return c.type == RenderPrimitiveType::ScissorStart &&
         c.data.scissor.x == x && c.data.scissor.y == y &&
         c.data.scissor.width == w && c.data.scissor.height == h;
}

TEST(label_text_is_scissored_to_its_own_box) {
  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 positioned(10, 10, 100, 30)
                     .with_label("a label far too long for this box")
                     .with_font(UIComponent::DEFAULT_FONT, pixels(16)));
  h.layout_only();
  const auto cmds = collect(h, box.ent());
  int scissor_at = -1, text_at = -1, end_at = -1;
  for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
    if (cmds[i].entity_id != box.id())
      continue;
    if (scissor_at < 0 && is_scissor(cmds[i], 10, 10, 100, 30))
      scissor_at = i;
    if (cmds[i].type == RenderPrimitiveType::Text && text_at < 0)
      text_at = i;
    if (text_at >= 0 && cmds[i].type == RenderPrimitiveType::ScissorEnd)
      end_at = i;
  }
  CHECK(scissor_at >= 0);
  CHECK(text_at > scissor_at);
  CHECK(end_at > text_at);
}

TEST(label_clip_restores_the_ancestor_clip) {
  ImmTestHarness h;
  auto parent = div(h.context(), mk(h.root(), 0),
                    positioned(0, 0, 200, 100).with_overflow(Overflow::Hidden));
  auto box = div(h.context(), mk(parent.ent(), 0),
                 positioned(10, 10, 100, 30)
                     .with_label("a label far too long for this box")
                     .with_font(UIComponent::DEFAULT_FONT, pixels(16)));
  h.layout_only();
  const auto cmds = collect(h, box.ent());
  int own_at = -1, text_at = -1, restore_at = -1, end_at = -1;
  for (int i = 0; i < static_cast<int>(cmds.size()); i++) {
    if (cmds[i].entity_id != box.id())
      continue;
    if (own_at < 0 && is_scissor(cmds[i], 10, 10, 100, 30))
      own_at = i;
    if (cmds[i].type == RenderPrimitiveType::Text && text_at < 0)
      text_at = i;
    if (text_at >= 0 && restore_at < 0 && is_scissor(cmds[i], 0, 0, 200, 100))
      restore_at = i;
    if (restore_at >= 0 && cmds[i].type == RenderPrimitiveType::ScissorEnd)
      end_at = i;
  }
  CHECK(own_at >= 0);
  CHECK(text_at > own_at);
  CHECK(restore_at > text_at);
  CHECK(end_at > restore_at);
}

TEST(wrapped_lines_that_do_not_fit_are_dropped_not_sliced) {
  ImmTestHarness h;
  auto box = div(h.context(), mk(h.root(), 0),
                 positioned(10, 10, 220, 28)
                     .with_label("Friend requests Friend requests")
                     .with_font(UIComponent::DEFAULT_FONT, pixels(18))
                     .with_text_overflow(TextOverflow::Wrap));
  h.layout_only();
  const auto cmds = collect(h, box.ent());
  int texts = 0;
  for (const auto &c : cmds) {
    if (c.entity_id != box.id() || c.type != RenderPrimitiveType::Text)
      continue;
    texts++;
    const auto &r = c.data.text.rect;
    CHECK(r.y >= 10.f - 0.5f);
    CHECK(r.y + r.height <= 10.f + 28.f + 0.5f);
  }
  CHECK(texts >= 1);
}

int main() { return ui_test::run_registered_tests("label clip"); }
