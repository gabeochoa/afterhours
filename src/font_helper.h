
#pragma once

#include <cstdlib>
#include <cstring>
#include <string>

#include "developer.h"

namespace afterhours {
#ifdef AFTER_HOURS_USE_RAYLIB
using Font = raylib::Font;
using vec2 = raylib::Vector2;
inline raylib::Font load_font_from_file(const char *file) {
    return raylib::LoadFont(file);
}

// Add codepoint-based font loading for CJK support
inline raylib::Font load_font_from_file_with_codepoints(const char *file,
                                                        int *codepoints,
                                                        int codepoint_count) {
    if (!file || !codepoints || codepoint_count <= 0) {
        log_error(
            "Invalid parameters for font loading: file={}, codepoints={}, "
            "count={}",
            file ? file : "null", codepoints ? "valid" : "null",
            codepoint_count);
        return raylib::GetFontDefault();
    }
    return raylib::LoadFontEx(file, 32, codepoints, codepoint_count);
}

// Utility function to remove duplicate codepoints from an array
// Returns a newly allocated array that must be freed by the caller
// The result count is written to codepointsResultCount
inline int *remove_duplicate_codepoints(int *codepoints, int codepointCount,
                                        int *codepointsResultCount) {
    if (!codepoints || codepointCount <= 0 || !codepointsResultCount) {
        if (codepointsResultCount) {
            *codepointsResultCount = 0;
        }
        return nullptr;
    }

    int codepointsNoDupsCount = codepointCount;
    int *codepointsNoDups =
        static_cast<int *>(calloc(codepointCount, sizeof(int)));
    if (!codepointsNoDups) {
        log_error("Failed to allocate memory for codepoint deduplication");
        *codepointsResultCount = 0;
        return nullptr;
    }

    memcpy(codepointsNoDups, codepoints, codepointCount * sizeof(int));

    // Remove duplicates
    for (int i = 0; i < codepointsNoDupsCount; i++) {
        for (int j = i + 1; j < codepointsNoDupsCount; j++) {
            if (codepointsNoDups[i] == codepointsNoDups[j]) {
                for (int k = j; k < codepointsNoDupsCount - 1; k++) {
                    codepointsNoDups[k] = codepointsNoDups[k + 1];
                }
                codepointsNoDupsCount--;
                j--;
            }
        }
    }

    // NOTE: The size of codepointsNoDups is the same as original array but
    // only required positions are filled (codepointsNoDupsCount)
    *codepointsResultCount = codepointsNoDupsCount;
    return codepointsNoDups;
}

// Convenience function to load a font with codepoints extracted from a string
// Automatically extracts codepoints, removes duplicates, and loads the font
// The caller is responsible for unloading the font when done
inline raylib::Font load_font_for_string(const std::string &content,
                                         const std::string &font_filename,
                                         int size = 96) {
    if (content.empty() || font_filename.empty()) {
        log_warn(
            "Empty content or font filename passed to load_font_for_string");
        return raylib::GetFontDefault();
    }

    int codepointCount = 0;
    int *codepoints = raylib::LoadCodepoints(content.c_str(), &codepointCount);

    if (!codepoints || codepointCount <= 0) {
        log_warn("Failed to extract codepoints from string");
        return raylib::GetFontDefault();
    }

    int codepointNoDupsCounts = 0;
    int *codepointsNoDups = remove_duplicate_codepoints(
        codepoints, codepointCount, &codepointNoDupsCounts);

    raylib::UnloadCodepoints(codepoints);

    if (!codepointsNoDups || codepointNoDupsCounts <= 0) {
        log_warn("Failed to process codepoints for font loading");
        return raylib::GetFontDefault();
    }

    raylib::Font font = raylib::LoadFontEx(
        font_filename.c_str(), size, codepointsNoDups, codepointNoDupsCounts);

    // Free the deduplicated codepoints array
    free(codepointsNoDups);

    return font;
}

inline float measure_text_internal(const char *content, const float size) {
    return static_cast<float>(
        raylib::MeasureText(content, static_cast<int>(size)));
}
inline raylib::Vector2 measure_text(const raylib::Font font, const char *content,
                         const float size, const float spacing) {
    return raylib::MeasureTextEx(font, content, size, spacing);
}

// Add proper UTF-8 text measurement for CJK support
inline raylib::Vector2 measure_text_utf8(const raylib::Font font, const char *content,
                              const float size, const float spacing) {
    if (!content) {
        log_warn("Null content passed to measure_text_utf8");
        return raylib::Vector2{0, 0};
    }

    if (size <= 0) {
        log_warn("Invalid font size {} passed to measure_text_utf8", size);
        return raylib::Vector2{0, 0};
    }

    // Use the existing measure_text for now, but this could be enhanced
    // to handle CJK text better with proper glyph spacing
    return raylib::MeasureTextEx(font, content, size, spacing);
}
#else
using Font = FontType;
inline Font load_font_from_file(const char *) { return Font(); }
inline Font load_font_from_file_with_codepoints(const char *, int *, int) {
    log_warn("Codepoint-based font loading not supported without raylib");
    return Font();
}

// Utility function to remove duplicate codepoints from an array
// Returns a newly allocated array that must be freed by the caller
// The result count is written to codepointsResultCount
inline int *remove_duplicate_codepoints(int * /*codepoints*/,
                                        int /*codepointCount*/,
                                        int *codepointsResultCount) {
    log_warn("Codepoint deduplication not supported without raylib");
    if (codepointsResultCount) {
        *codepointsResultCount = 0;
    }
    return nullptr;
}

// Convenience function to load a font with codepoints extracted from a string
inline Font load_font_for_string(const std::string &, const std::string &,
                                 int = 96) {
    log_warn("load_font_for_string not supported without raylib");
    return Font();
}
inline float measure_text_internal(const char *, const float) {
    log_warn(
        "Text size measuring not supported. Either use "
        "AFTER_HOURS_USE_RAYLIB or provide your own through "
        "set_measure_text_fn()");
    return 0.f;
}
inline Vector2Type measure_text(const Font, const char *, const float /*size*/,
                                const float /*spacing*/) {
    log_warn(
        "Text size measuring not supported. Either use "
        "AFTER_HOURS_USE_RAYLIB or provide your own through "
        "set_measure_text_fn()");
    return Vector2Type{0, 0};
}
inline Vector2Type measure_text_utf8(const Font, const char *,
                                     const float /*size*/,
                                     const float /*spacing*/) {
    log_warn(
        "UTF-8 text measuring not supported. Either use "
        "AFTER_HOURS_USE_RAYLIB or provide your own through "
        "set_measure_text_fn()");
    return Vector2Type{0, 0};
}
#endif
}  // namespace afterhours
