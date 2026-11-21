#include <afterhours/src/font_catalog.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace fc = afterhours::font_catalog;

static int failures = 0;
#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #cond);             \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

int main() {
  // Empty / missing dirs are a supported, empty answer — not an error.
  const auto empty = fc::list_fonts_in({});
  CHECK(empty.supported && empty.faces.empty());
  const auto missing = fc::list_fonts_in({"/nonexistent-font-dir-r084"});
  CHECK(missing.supported && missing.faces.empty());

  const auto dirs = fc::system_font_dirs();
  CHECK(!dirs.empty());
  for (const auto &d : dirs)
    std::printf("  dir: %s\n", d.c_str());

#if defined(__APPLE__)
  // This machine's real catalog: many faces, and families every macOS
  // install ships. Guarded to Apple so other platforms only assert the
  // generic contract below.
  const auto catalog = fc::list_system_fonts();
  CHECK(catalog.supported);
  CHECK(catalog.faces.size() > 100);
  bool helvetica = false, courier = false;
  for (const auto &f : catalog.faces) {
    if (f.family == "Helvetica" || f.family == "Helvetica Neue")
      helvetica = true;
    if (f.family.find("Courier") != std::string::npos)
      courier = true;
  }
  std::printf("  system faces: %zu (Helvetica %d, Courier %d)\n",
              catalog.faces.size(), helvetica, courier);
  CHECK(helvetica);
  CHECK(courier);

  // A known collection on every macOS install: Courier.ttc has 4 faces.
  const std::string ttc = "/System/Library/Fonts/Courier.ttc";
  if (std::filesystem::exists(ttc)) {
    CHECK(fc::face_count_for_file(ttc) == 4);
    const auto faces = fc::faces_for_file(ttc);
    CHECK(faces.size() == 4 && faces[0].collection);
    CHECK(faces[0].face_index == 0 && faces[3].face_index == 3);
    bool named = false;
    for (const auto &f : faces)
      named |= f.family.find("Courier") != std::string::npos;
    CHECK(named);
    if (!faces.empty())
      std::printf("  %s face0: %s / %s\n", ttc.c_str(),
                  faces[0].family.c_str(), faces[0].style.c_str());
  }

  const std::string single = "/System/Library/Fonts/Geneva.ttf";
  if (std::filesystem::exists(single)) {
    CHECK(fc::face_count_for_file(single) == 1);
    const auto faces = fc::faces_for_file(single);
    CHECK(faces.size() == 1 && !faces[0].collection);
    CHECK(faces[0].family.find("Geneva") != std::string::npos);
  }
#else
  const auto catalog = fc::list_system_fonts();
  CHECK(catalog.supported);
  std::printf("  system faces: %zu\n", catalog.faces.size());
#endif

  CHECK(fc::face_count_for_file("/nonexistent/font.ttf") == 0);
  const unsigned char junk[32] = {};
  CHECK(fc::faces_in_bytes(junk, sizeof(junk)).empty());
  CHECK(fc::faces_in_bytes(nullptr, 0).empty());

  if (failures) {
    std::fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  std::printf("font_catalog_test: all checks passed\n");
  return 0;
}
