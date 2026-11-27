// instance_batcher_test.cpp
// InstanceBatcher and model_transform under raylib:
//   - model_transform matches raymath's scale * rotateY * translate, the
//     composition DrawModelEx uses
//   - copies of two models land in two groups, and flush empties them
//   - a batched frame renders the same pixels as one draw_model per copy
//
// Opens a hidden window: models and render targets need a GL context.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace raylib {
#include <raylib.h>
// raymath zero-initialises with `= { 0 }`, which -Wextra flags once per use.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#include <raymath.h>
#pragma clang diagnostic pop
} // namespace raylib

#include <afterhours/ah.h>
#include <afterhours/src/graphics.h>
#include <afterhours/src/plugins/models.h>

using namespace afterhours;

static int checks_run = 0;
static int checks_passed = 0;

static void check(bool cond, const std::string &what) {
  checks_run++;
  if (cond) {
    checks_passed++;
  } else {
    fprintf(stderr, "  FAIL: %s\n", what.c_str());
  }
}

static float max_abs_diff(const raylib::Matrix &a, const raylib::Matrix &b) {
  const float *pa = &a.m0, *pb = &b.m0;
  float worst = 0.f;
  for (int i = 0; i < 16; i++) worst = std::fmax(worst, std::fabs(pa[i] - pb[i]));
  return worst;
}

int main() {
  // model_transform against raymath.
  {
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> unit(-1.f, 1.f);
    float worst = 0.f;
    for (int i = 0; i < 1000; i++) {
      raylib::Vector3 pos = {unit(rng) * 50, unit(rng) * 50, unit(rng) * 50};
      raylib::Vector3 scale = {1.5f + unit(rng), 1.5f + unit(rng), 1.5f + unit(rng)};
      float yaw = unit(rng) * 720.f;
      raylib::Matrix expected = raylib::MatrixMultiply(
          raylib::MatrixMultiply(raylib::MatrixScale(scale.x, scale.y, scale.z),
                                 raylib::MatrixRotateY(yaw * DEG2RAD)),
          raylib::MatrixTranslate(pos.x, pos.y, pos.z));
      worst = std::fmax(worst, max_abs_diff(model_transform(pos, yaw, scale), expected));
    }
    check(worst < 1e-4f, "model_transform matches raymath");
  }

  raylib::SetTraceLogLevel(raylib::LOG_ERROR);
  afterhours::graphics::Config cfg;
  cfg.display = afterhours::graphics::DisplayMode::Headless;
  cfg.width = 64;
  cfg.height = 64;
  if (!afterhours::graphics::init(cfg)) {
    fprintf(stderr, "instance_batcher_test: headless graphics init failed\n");
    return 1;
  }
  auto &models = ModelLibrary::get();
  check(models.load(AFTERHOURS_MODELS_DIR "/tree-large.glb", "tree"), "tree loads");
  check(models.load(AFTERHOURS_MODELS_DIR "/kaykit/lantern.gltf", "lantern"),
        "lantern loads");
  if (!models.contains("tree") || !models.contains("lantern")) {
    afterhours::graphics::shutdown();
    printf("instance_batcher_test: %d/%d checks passed\n", checks_passed, checks_run);
    return 1;
  }

  const ModelType &tree = models.get("tree");
  const ModelType &lantern = models.get("lantern");

  // Grouping.
  InstanceBatcher batcher;
  raylib::Shader shader = load_instancing_shader();
  struct Copy {
    const ModelType *model;
    raylib::Vector3 pos;
    float yaw;
    float scale;
  } scene[] = {
      {&tree, {-3, 0, 0}, 0.f, 1.f},     {&lantern, {-1, 0, 1}, 30.f, 1.f},
      {&tree, {1, 0, -1}, 90.f, 0.8f},   {&lantern, {3, 0, 0}, 200.f, 1.2f},
      {&tree, {0, 0, 2}, 45.f, 1.1f},
  };
  for (const Copy &c : scene)
    batcher.add(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
  check(batcher.copies() == 5, "every copy is collected");
  check(batcher.group_count() == 2, "copies group by model");

  // Keys split one model's copies; on_group sees each group once, in
  // first-seen order, before it draws.
  {
    InstanceBatcher keyed;
    keyed.add(tree, {0, 0, 0}, 0.f, {1, 1, 1}, 0);
    keyed.add(tree, {1, 0, 0}, 0.f, {1, 1, 1}, 1);
    keyed.add(lantern, {2, 0, 0}, 0.f, {1, 1, 1}, 1);
    keyed.add(tree, {3, 0, 0}, 0.f, {1, 1, 1}, 1);
    check(keyed.group_count() == 3, "a key splits one model into two groups");
    std::string seen;
    raylib::RenderTexture2D scratch = raylib::LoadRenderTexture(8, 8);
    raylib::BeginTextureMode(scratch);
    keyed.flush(shader, [&](const ModelType &m, uint32_t key,
                            std::span<const MatrixType> ts) {
      seen += std::string(&m == &tree ? "tree" : "lantern") + ":" +
              std::to_string(key) + "x" + std::to_string(ts.size()) + " ";
    });
    raylib::EndTextureMode();
    raylib::UnloadRenderTexture(scratch);
    check(seen == "tree:0x1 tree:1x2 lantern:1x1 ",
          "on_group runs per group with its key and transforms: " + seen);
    check(keyed.copies() == 0, "a flush with on_group empties the groups");
  }

  {
    std::vector<MatrixType> caller{
        model_transform({-3, 0, 0}, 0.f, {1, 1, 1}),
        model_transform({1, 0, -1}, 90.f, {0.8f, 0.8f, 0.8f}),
        model_transform({0, 0, 2}, 45.f, {1.1f, 1.1f, 1.1f})};
    InstanceBatcher borrowed;
    borrowed.borrow(tree, caller);
    check(borrowed.copies() == 3 && borrowed.group_count() == 1,
          "borrowed copies count without being copied in");
    const MatrixType *seen_ptr = nullptr;
    size_t seen_n = 0;
    raylib::RenderTexture2D scratch = raylib::LoadRenderTexture(8, 8);
    raylib::BeginTextureMode(scratch);
    borrowed.flush(shader, [&](const ModelType &, uint32_t,
                             std::span<const MatrixType> ts) {
      seen_ptr = ts.data();
      seen_n = ts.size();
      check(ts.size() == 3 && max_abs_diff(ts[0], caller[0]) < 1e-6f &&
                max_abs_diff(ts[2], caller[2]) < 1e-6f,
            "borrowed span carries the caller's transforms");
    });
    raylib::EndTextureMode();
    raylib::UnloadRenderTexture(scratch);
    check(seen_ptr == caller.data() && seen_n == 3,
          "borrowed flush hands back the caller's own span, uncopied");
    check(borrowed.copies() == 0, "a borrowed flush empties the group");

    InstanceBatcher mixed;
    mixed.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    mixed.borrow(tree, caller);
    mixed.borrow(lantern, std::span<const MatrixType>(caller.data(), 2), 1);
    check(mixed.copies() == 6 && mixed.group_count() == 2,
          "mixed owned and borrowed copies count together");
    std::string segments;
    raylib::RenderTexture2D scratch2 = raylib::LoadRenderTexture(8, 8);
    raylib::BeginTextureMode(scratch2);
    mixed.flush(shader, [&](const ModelType &m, uint32_t key,
                          std::span<const MatrixType> ts) {
      segments += std::string(&m == &tree ? "tree" : "lantern") + ":" +
                  std::to_string(key) + "x" + std::to_string(ts.size()) +
                  (ts.data() == caller.data() ? "B" : "O") + " ";
    });
    raylib::EndTextureMode();
    raylib::UnloadRenderTexture(scratch2);
    check(segments == "tree:0x1O tree:0x3B lantern:1x2B ",
          "mixed group flushes owned then borrowed segments: " + segments);
    check(mixed.copies() == 0, "a mixed flush empties the groups");
    auto fill = [&](InstanceBatcher &b) {
      for (const Copy &c : scene)
        b.add(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
      b.add(tree, {9, 9, 9}, 0.f, {1, 1, 1}, 7);
    };
    InstanceBatcher drained, flushed;
    fill(drained);
    fill(flushed);
    auto collect = [&](InstanceBatcher &b, bool do_drain) {
      std::string seen;
      std::vector<raylib::Matrix> mats;
      auto visit = [&](const ModelType &m, uint32_t key,
                       std::span<const MatrixType> ts) {
        seen += std::string(&m == &tree ? "tree" : "lantern") + ":" +
                std::to_string(key) + "x" + std::to_string(ts.size()) + ":";
        for (const MatrixType &t : ts) {
          seen += std::to_string(t.m12) + "," + std::to_string(t.m14) + ";";
          mats.push_back(t);
        }
      };
      if (do_drain) {
        b.drain(visit);
      } else {
        raylib::RenderTexture2D scratch = raylib::LoadRenderTexture(8, 8);
        raylib::BeginTextureMode(scratch);
        b.flush(shader, visit);
        raylib::EndTextureMode();
        raylib::UnloadRenderTexture(scratch);
      }
      return std::pair{seen, mats};
    };
    auto [drained_seen, drained_mats] = collect(drained, true);
    auto [flushed_seen, flushed_mats] = collect(flushed, false);
    check(drained_seen == flushed_seen,
          "drain groups match what flush would draw: " + drained_seen);
    check(drained_mats.size() == 6 && flushed_mats.size() == 6,
          "drain count matches");
    bool same = drained_mats.size() == flushed_mats.size();
    for (size_t i = 0; same && i < drained_mats.size(); i++)
      same = max_abs_diff(drained_mats[i], flushed_mats[i]) < 1e-6f;
    check(same, "drained transforms match flush transforms");
    check(drained.copies() == 0 && drained.group_count() == 0,
          "batcher is empty after drain");
    drained.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    check(drained.copies() == 1 && drained.group_count() == 1,
          "batcher is reusable after drain");
    drained.clear();

    // drain must emit borrowed-only groups too: borrowed spans are stored
    // separately from owned transforms, so skipping empty-transform groups
    // would strand them and copies() would never reach 0.
    {
      InstanceBatcher drain_borrowed;
      drain_borrowed.borrow(tree, caller);
      check(drain_borrowed.copies() == 3 && drain_borrowed.group_count() == 1,
            "borrowed-only group counts before drain");
      std::string drain_segments;
      const MatrixType *drain_ptr = nullptr;
      drain_borrowed.drain([&](const ModelType &m, uint32_t key,
                               std::span<const MatrixType> ts) {
        drain_segments += std::string(&m == &tree ? "tree" : "lantern") +
                          ":" + std::to_string(key) + "x" +
                          std::to_string(ts.size()) +
                          (ts.data() == caller.data() ? "B " : "O ");
        drain_ptr = ts.data();
      });
      check(drain_segments == "tree:0x3B ",
            "drain emits a borrowed-only group as-is: " + drain_segments);
      check(drain_ptr == caller.data(),
            "drain hands back the caller's own span, uncopied");
      check(drain_borrowed.copies() == 0 && drain_borrowed.group_count() == 0,
            "copies reach 0 after draining a borrowed-only group");
      check(caller.size() == 3,
            "drain never frees or consumes the caller's buffer");
    }

    // Mixed owned+borrowed in the same group drain as owned first, then
    // each borrowed span, and tint state resets like clear() (#310).
    {
      InstanceBatcher drain_mixed;
      drain_mixed.add(tree, {-1, 0, 0}, 0.f, {1, 1, 1}, 0,
                      Color{255, 0, 0, 255});
      drain_mixed.borrow(tree, caller);
      check(drain_mixed.copies() == 4 && drain_mixed.group_count() == 1,
            "mixed owned+borrowed same group counts before drain");
      std::string drain_segments;
      drain_mixed.drain([&](const ModelType &m, uint32_t key,
                            std::span<const MatrixType> ts) {
        drain_segments += std::string(&m == &tree ? "tree" : "lantern") +
                          ":" + std::to_string(key) + "x" +
                          std::to_string(ts.size()) +
                          (ts.data() == caller.data() ? "B " : "O ");
      });
      check(drain_segments == "tree:0x1O tree:0x3B ",
            "drain emits mixed group owned then borrowed: " +
                drain_segments);
      check(drain_mixed.copies() == 0 && drain_mixed.group_count() == 0,
            "copies reach 0 after draining a mixed group");
      drain_mixed.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
      check(drain_mixed.copies() == 1,
            "batcher is reusable after draining a mixed group");
      drain_mixed.clear();
      check(drain_mixed.copies() == 0, "clear empties a refilled group");
    }
  }

  // Same frame two ways: batched, and one draw_model per copy.
  raylib::Camera3D cam{};
  cam.position = {0, 6, 10};
  cam.target = {0, 1, 0};
  cam.up = {0, 1, 0};
  cam.fovy = 45.f;
  cam.projection = raylib::CAMERA_PERSPECTIVE;
  raylib::RenderTexture2D rt = raylib::LoadRenderTexture(256, 256);
  auto render = [&](bool batched) {
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    if (batched) {
      batcher.flush(shader);
    } else {
      for (const Copy &c : scene)
        draw_model(*c.model, c.pos, c.yaw, {c.scale, c.scale, c.scale});
    }
    raylib::EndMode3D();
    raylib::EndTextureMode();
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    return img;
  };
  raylib::Image batched = render(true);
  check(batcher.copies() == 0 && batcher.group_count() == 0,
        "flush empties the groups");
  raylib::Image direct = render(false);

  auto *pa = static_cast<raylib::Color *>(batched.data);
  auto *pb = static_cast<raylib::Color *>(direct.data);
  int covered = 0, differ = 0;
  for (int i = 0; i < 256 * 256; i++) {
    if (pa[i].a > 0 || pb[i].a > 0) covered++;
    int d = std::abs(pa[i].r - pb[i].r) + std::abs(pa[i].g - pb[i].g) +
            std::abs(pa[i].b - pb[i].b) + std::abs(pa[i].a - pb[i].a);
    if (d > 6) differ++;
  }

  printf("  %d covered pixels, %d differ\n", covered, differ);
  check(covered > 2000, "the scene is on screen");
  check(differ <= covered / 40, "batched frame matches draw_model per copy");

  {
    std::vector<MatrixType> tree_ts, lantern_ts;
    for (const Copy &c : scene)
      (c.model == &tree ? tree_ts : lantern_ts)
          .push_back(model_transform(c.pos, c.yaw, {c.scale, c.scale, c.scale}));
    InstanceBatcher bb;
    bb.borrow(tree, tree_ts);
    bb.borrow(lantern, lantern_ts);
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    bb.flush(shader);
    raylib::EndMode3D();
    raylib::EndTextureMode();
    check(bb.copies() == 0, "borrowed flush empties before the pixel check");
    raylib::Image borrowed_img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&borrowed_img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *pc = static_cast<raylib::Color *>(borrowed_img.data);
    int covered_b = 0, differ_b = 0;
    for (int i = 0; i < 256 * 256; i++) {
      if (pc[i].a > 0 || pb[i].a > 0) covered_b++;
      int d = std::abs(pc[i].r - pb[i].r) + std::abs(pc[i].g - pb[i].g) +
              std::abs(pc[i].b - pb[i].b) + std::abs(pc[i].a - pb[i].a);
      if (d > 6) differ_b++;
    }
    printf("  %d covered pixels, %d differ (borrowed)\n", covered_b, differ_b);
    check(covered_b > 2000, "the borrowed scene is on screen");
    check(differ_b <= covered_b / 40,
          "borrowed frame matches draw_model per copy");
    raylib::UnloadImage(borrowed_img);
  }

  raylib::UnloadImage(batched);
  raylib::UnloadImage(direct);

  {
    struct TintedCopy {
      raylib::Vector3 pos;
      Color tint;
    };
    TintedCopy tinted_scene[] = {{{-2, 0, 0}, {255, 0, 0, 255}},
                                 {{2, 0, 0}, {0, 0, 255, 255}}};
    InstanceBatcher tinted;
    for (const TintedCopy &c : tinted_scene)
      tinted.add(tree, c.pos, 0.f, {1, 1, 1}, 0, c.tint);
    check(tinted.copies() == 2 && tinted.group_count() == 1,
          "tinted copies group by model");
    auto render_tinted = [&](bool batched_tint) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (batched_tint) {
        tinted.flush(shader);
      } else {
        for (const TintedCopy &c : tinted_scene)
          draw_model(tree, c.pos, 0.f, {1, 1, 1}, c.tint);
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image tint_batched = render_tinted(true);
    raylib::Image tint_direct = render_tinted(false);
    auto *pt = static_cast<raylib::Color *>(tint_batched.data);
    auto *pd = static_cast<raylib::Color *>(tint_direct.data);
    int tint_differ = 0, tint_covered = 0;
    long left_r = 0, left_b = 0, right_r = 0, right_b = 0, left_n = 0, right_n = 0;
    for (int y = 0; y < 256; y++) {
      for (int x = 0; x < 256; x++) {
        int i = y * 256 + x;
        if (pt[i].a > 0 || pd[i].a > 0) tint_covered++;
        int d = std::abs(pt[i].r - pd[i].r) + std::abs(pt[i].g - pd[i].g) +
                std::abs(pt[i].b - pd[i].b) + std::abs(pt[i].a - pd[i].a);
        if (d > 6) tint_differ++;
        if (pt[i].a > 0) {
          if (x < 128) {
            left_r += pt[i].r; left_b += pt[i].b; left_n++;
          } else {
            right_r += pt[i].r; right_b += pt[i].b; right_n++;
          }
        }
      }
    }
    printf("  tint: %d covered, %d differ, left R-B %ld, right R-B %ld\n",
           tint_covered, tint_differ,
           left_n ? (left_r - left_b) / left_n : 0,
           right_n ? (right_r - right_b) / right_n : 0);
    check(tint_covered > 1000, "tinted scene is on screen");
    check(tint_differ <= tint_covered / 40,
          "tinted batched frame matches draw_model with tints");
    check(left_n > 0 && right_n > 0 && left_r > left_b && right_b > right_r,
          "per-instance tint reaches pixels (red left, blue right)");
    raylib::UnloadImage(tint_batched);
    raylib::UnloadImage(tint_direct);
  }

  // Borrowed segment in a tinted group: it has no tint data, so it must
  // draw through the tinted path with white tints — matching an untinted
  // draw_model on the right, not the owned copy's red on the left.
  {
    std::vector<MatrixType> borrowed_ts{
        model_transform({2, 0, 0}, 0.f, {1, 1, 1})};
    InstanceBatcher tinted_borrow;
    tinted_borrow.add(tree, {-2, 0, 0}, 0.f, {1, 1, 1}, 0,
                      Color{255, 0, 0, 255});
    tinted_borrow.borrow(tree, borrowed_ts);
    check(tinted_borrow.copies() == 2 && tinted_borrow.group_count() == 1,
          "tinted owned + borrowed copies share one group");
    auto render_tinted_borrow = [&](bool batched_mode) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (batched_mode) {
        tinted_borrow.flush(shader);
      } else {
        draw_model(tree, {-2, 0, 0}, 0.f, {1, 1, 1}, Color{255, 0, 0, 255});
        draw_model(tree, {2, 0, 0}, 0.f, {1, 1, 1});
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image tb_batched = render_tinted_borrow(true);
    check(tinted_borrow.copies() == 0,
          "tinted borrowed flush empties the group");
    raylib::Image tb_direct = render_tinted_borrow(false);
    auto *ptb = static_cast<raylib::Color *>(tb_batched.data);
    auto *ptd = static_cast<raylib::Color *>(tb_direct.data);
    int tb_covered = 0, tb_differ = 0;
    long tb_left_rb = 0, tb_right_rb = 0, tb_left_n = 0, tb_right_n = 0;
    for (int y = 0; y < 256; y++) {
      for (int x = 0; x < 256; x++) {
        int i = y * 256 + x;
        if (ptb[i].a > 0 || ptd[i].a > 0) tb_covered++;
        int d = std::abs(ptb[i].r - ptd[i].r) + std::abs(ptb[i].g - ptd[i].g) +
                std::abs(ptb[i].b - ptd[i].b) + std::abs(ptb[i].a - ptd[i].a);
        if (d > 6) tb_differ++;
        if (ptb[i].a > 0) {
          if (x < 128) {
            tb_left_rb += ptb[i].r - ptb[i].b;
            tb_left_n++;
          } else {
            tb_right_rb += ptb[i].r - ptb[i].b;
            tb_right_n++;
          }
        }
      }
    }
    printf("  tinted-borrow: %d covered, %d differ, left R-B %ld, right "
           "R-B %ld\n",
           tb_covered, tb_differ, tb_left_n ? tb_left_rb / tb_left_n : 0,
           tb_right_n ? tb_right_rb / tb_right_n : 0);
    check(tb_covered > 1000, "tinted borrowed scene is on screen");
    check(tb_differ <= tb_covered / 40,
          "borrowed segment in a tinted group matches untinted draw_model");
    check(tb_left_n > 0 && tb_right_n > 0 &&
              tb_left_rb / tb_left_n > tb_right_rb / tb_right_n,
          "owned copy is redder than the white-tinted borrowed copy");
    raylib::UnloadImage(tb_batched);
    raylib::UnloadImage(tb_direct);
  }

  {
    InstanceBatcher mixed;
    mixed.add(tree, {-3, 0, 0}, 0.f, {0.8f, 0.8f, 0.8f});
    mixed.add(lantern, {3, 0, 0}, 0.f, {1, 1, 1});
    auto render_mixed = [&](bool mixed_mode) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (mixed_mode) {
        mixed.flush(shader);
        draw_model(tree, {0, 0, 0}, 45.f, {0.7f, 0.7f, 0.7f});
        mixed.add(tree, {0, 0, 2.5f}, 90.f, {0.6f, 0.6f, 0.6f});
        mixed.flush(shader);
      } else {
        draw_model(tree, {-3, 0, 0}, 0.f, {0.8f, 0.8f, 0.8f});
        draw_model(lantern, {3, 0, 0}, 0.f, {1, 1, 1});
        draw_model(tree, {0, 0, 0}, 45.f, {0.7f, 0.7f, 0.7f});
        draw_model(tree, {0, 0, 2.5f}, 90.f, {0.6f, 0.6f, 0.6f});
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image mixed_img = render_mixed(true);
    raylib::Image plain_img = render_mixed(false);
    auto *pm = static_cast<raylib::Color *>(mixed_img.data);
    auto *pp = static_cast<raylib::Color *>(plain_img.data);
    int mixed_covered = 0, mixed_differ = 0;
    for (int i = 0; i < 256 * 256; i++) {
      if (pm[i].a > 0 || pp[i].a > 0) mixed_covered++;
      int d = std::abs(pm[i].r - pp[i].r) + std::abs(pm[i].g - pp[i].g) +
              std::abs(pm[i].b - pp[i].b) + std::abs(pm[i].a - pp[i].a);
      if (d > 6) mixed_differ++;
    }
    printf("  mixed: %d covered, %d differ\n", mixed_covered, mixed_differ);
    check(mixed_covered > 1500, "mixed instanced+plain scene is on screen");
    check(mixed_differ <= mixed_covered / 40,
          "instanced, plain, instanced sequence matches all-plain");
    raylib::UnloadImage(mixed_img);
    raylib::UnloadImage(plain_img);
  }

  {
    InstanceBatcher owned;
    owned.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    raylib::BeginTextureMode(rt);
    raylib::ClearBackground({0, 0, 0, 0});
    raylib::BeginMode3D(cam);
    owned.flush();
    raylib::EndMode3D();
    raylib::EndTextureMode();
    check(owned.copies() == 0, "flush() with owned default shader empties groups");
    raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
    raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    auto *po = static_cast<raylib::Color *>(img.data);
    int owned_covered = 0;
    for (int i = 0; i < 256 * 256; i++)
      if (po[i].a > 0) owned_covered++;
    printf("  owned shader: %d covered\n", owned_covered);
    check(owned_covered > 500, "owned default shader draws the scene");
    raylib::UnloadImage(img);
  }

  // Draining a tinted group must reset tint state like clear() does: an
  // untinted copy added after the drain draws white, not the stale tint.
  {
    InstanceBatcher b;
    b.add(tree, {0, 0, 0}, 0.f, {1, 1, 1}, 0, Color{255, 0, 0, 255});
    b.add(tree, {2, 0, 0}, 0.f, {1, 1, 1}, 0, Color{0, 0, 255, 255});
    b.drain([](const ModelType &, uint32_t,
               std::span<const MatrixType>) {});
    check(b.copies() == 0 && b.group_count() == 0,
          "drain empties a tinted group");
    b.add(tree, {0, 0, 0}, 0.f, {1, 1, 1});
    auto render_after_drain = [&](bool batched_after) {
      raylib::BeginTextureMode(rt);
      raylib::ClearBackground({0, 0, 0, 0});
      raylib::BeginMode3D(cam);
      if (batched_after) {
        b.flush(shader);
      } else {
        draw_model(tree, {0, 0, 0}, 0.f, {1, 1, 1});
      }
      raylib::EndMode3D();
      raylib::EndTextureMode();
      raylib::Image img = raylib::LoadImageFromTexture(rt.texture);
      raylib::ImageFormat(&img, raylib::PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
      return img;
    };
    raylib::Image after_batched = render_after_drain(true);
    raylib::Image after_direct = render_after_drain(false);
    auto *pab = static_cast<raylib::Color *>(after_batched.data);
    auto *pad = static_cast<raylib::Color *>(after_direct.data);
    int after_covered = 0, after_differ = 0;
    for (int i = 0; i < 256 * 256; i++) {
      if (pab[i].a > 0 || pad[i].a > 0) after_covered++;
      int d = std::abs(pab[i].r - pad[i].r) + std::abs(pab[i].g - pad[i].g) +
              std::abs(pab[i].b - pad[i].b) + std::abs(pab[i].a - pad[i].a);
      if (d > 6) after_differ++;
    }
    printf("  drain-tint: %d covered, %d differ\n", after_covered, after_differ);
    check(after_covered > 500, "untinted copy after drain is on screen");
    check(after_differ <= after_covered / 40,
          "untinted copy after a tinted drain draws untinted");
    raylib::UnloadImage(after_batched);
    raylib::UnloadImage(after_direct);
  }

  raylib::UnloadRenderTexture(rt);
  raylib::UnloadShader(shader);
  models.unload_all();
  afterhours::graphics::shutdown();

  printf("instance_batcher_test: %d/%d checks passed\n", checks_passed, checks_run);
  return checks_passed == checks_run ? 0 : 1;
}
