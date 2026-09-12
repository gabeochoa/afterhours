# Render capture formats

Use `capture_render_texture_rgba` for owned RGBA8 pixels and physical pixel
dimensions. Rows run from top to bottom; each row contains width times four
bytes in red, green, blue, alpha order, without padding. Values are the stored
render-target channels. The operation does not change their alpha convention.

Use `capture_render_texture_png` for owned encoded PNG bytes. Both return
`std::optional` and report failure with `std::nullopt`. The backend with no
graphics support returns failure. Invalid or unloaded targets fail rather
than returning an empty successful image.

The older `capture_render_texture_to_memory` now consistently returns encoded
PNG on both raylib and Metal, or an empty buffer on failure. Metal callers that
previously indexed its raw bytes must move to `capture_render_texture_rgba`.
The library's Metal tests have been migrated. The reviewed external caller in
endless-dance-chaos supplies PNG screenshots to MCP and retains that behavior.

Backend tests capture the same nonsquare target with a translucent red top
and opaque blue bottom. They verify dimensions, channel order, orientation,
alpha, legacy PNG signatures, byte-for-byte PNG decode versus RGBA, and failed
readback. No CPU image editing or mutable texture upload API is added here.
