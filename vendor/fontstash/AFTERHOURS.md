# Afterhours changes to Fontstash

This directory contains an altered Fontstash source distribution. The original
license notice remains in `fontstash.h`.

Afterhours adds `fonsTextAdvance(context, string, end)`. It obtains advances from
resident glyph records or font metrics without allocating glyph images in the
atlas. It preserves the existing size quantization, fallback selection, kerning
and advance rounding. It returns advance width only; it does not replace ink
bounds or change the behavior of `fonsTextBounds`, `fonsDrawText` or text iterators.
The Sokol backend uses it for `measure_text`.

See `docs/font-atlas-measurement.md` at the afterhours repository root for the
regression, benchmark and remaining limitations.
