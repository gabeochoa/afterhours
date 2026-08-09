#pragma once

// Emscripten-only helpers. Included from graphics.h under __EMSCRIPTEN__.
// Pair with tools/web.mk + tools/web/shell.html 

#include <emscripten.h>

namespace afterhours::graphics {

// Match the browser client area so a CSS-full canvas has a 1:1 backing store.
inline void web_fit_canvas_to_browser() {
  if (!is_window_ready())
    return;
  const int want_w = EM_ASM_INT({ return window.innerWidth | 0; });
  const int want_h = EM_ASM_INT({ return window.innerHeight | 0; });
  if (want_w <= 0 || want_h <= 0)
    return;
  if (get_screen_width() != want_w || get_screen_height() != want_h)
    set_window_size(want_w, want_h);
}

// Enter browser fullscreen immediately. Must run under a user gesture (e.g.
// Apply click). Prefer over raylib ToggleFullscreen — that defers via
// setTimeout and browsers reject it.
inline void web_request_fullscreen_now() {
  EM_ASM({
    var el = document.documentElement;
    var req = el.requestFullscreen || el.webkitRequestFullscreen ||
              el.msRequestFullscreen;
    if (req)
      req.call(el);
    if (typeof Module !== 'undefined')
      Module.pendingFullscreen = false;
  });
}

inline void web_exit_fullscreen() {
  EM_ASM({
    if (typeof Module !== 'undefined')
      Module.pendingFullscreen = false;
    if (document.exitFullscreen)
      document.exitFullscreen();
    else if (document.webkitExitFullscreen)
      document.webkitExitFullscreen();
  });
}

// Arm pending fullscreen for the next click/key (shell fulfills it), or exit.
inline void web_apply_fullscreen(bool want) {
  const bool is_fs =
      EM_ASM_INT({ return document.fullscreenElement ? 1 : 0; }) != 0;
  if (want && !is_fs) {
    EM_ASM({
      if (typeof Module !== 'undefined')
        Module.pendingFullscreen = true;
    });
  } else if (!want && is_fs) {
    web_exit_fullscreen();
  } else if (is_fs) {
    EM_ASM({
      if (typeof Module !== 'undefined')
        Module.pendingFullscreen = false;
    });
  }
}

inline bool web_is_fullscreen() {
  return EM_ASM_INT({ return document.fullscreenElement ? 1 : 0; }) != 0;
}

} // namespace afterhours::graphics
