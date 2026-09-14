# Font measurement fixtures

These unmodified fonts exercise real glyph metrics, kerning and fallback selection
without depending on installed system fonts or a graphics context.

- `AtkinsonHyperlegible-Regular.ttf`: Braille Institute, SIL Open Font License.
  License: `Atkinson-OFL.txt`.
- `ArchivoNarrow-Regular.ttf`: Archivo Narrow, SIL Open Font License.
  License: `ARCHIVO_NARROW_OFL.txt`.

Both files and their licenses were copied from the existing WM resources/fonts
collection. The test uses a codepoint present in Archivo but absent from Atkinson
to exercise a real fallback lookup.
