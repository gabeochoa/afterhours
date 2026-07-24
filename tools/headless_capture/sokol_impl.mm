// tools/headless_capture/sokol_impl.mm
// The single translation unit that compiles the sokol + fontstash
// implementations plus the afterhours Metal capture path for the headless
// harness. Compiled as Objective-C++ so sokol selects Metal and
// CoreGraphics/ImageIO are available.
//
// We pull in sokol_app / sokol_glue even though the harness never opens a
// window: afterhours' MetalPlatformAPI (included transitively via graphics.h)
// references sapp_*/sglue_* symbols. SOKOL_NO_ENTRY means sokol does not define
// main() -- the harness provides its own. We simply never call sapp_run().

#define SOKOL_IMPL
#define SOKOL_NO_ENTRY

#include <sokol/sokol_app.h>
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_glue.h>
#include <sokol/sokol_log.h>
#include <sokol/sokol_time.h>
#include <sokol/sokol_gl.h>

// Text rendering: fontstash + the sokol integration (for sfons_flush, which
// begin_texture_mode/end_texture_mode reference).
#define FONTSTASH_IMPLEMENTATION
#include <fontstash/fontstash.h>
#include <sokol/sokol_fontstash.h>

// The Metal texture readback + PNG export (metal_capture_render_texture) that
// capture_render_texture() calls. Guarded on AFTER_HOURS_USE_METAL, and needs
// sokol_gfx (with SOKOL_IMPL) already included above.
#define AFTER_HOURS_USE_METAL
#include <afterhours/src/backends/sokol/capture_impl.h>
