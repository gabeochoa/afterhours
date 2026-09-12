# Input binding persistence

Include `src/plugins/input_binding_codec.h`. `encode` and `decode` handle one
binding; `encode_bindings` and `decode_bindings` handle an ordered binding list.
Decoding returns `std::nullopt` for malformed data, unsupported versions/tags,
unknown key/button/axis codes, invalid modifiers or invalid directions.

The format uses versioned strings, suitable for a JSON array or another
application-owned container. For example, `v1:key:65:3:1` records A with
Shift+Ctrl and explicit modifiers. `v1:key:65:0:0` is a permissive A binding;
`v1:key:65:0:1` requires no modifiers. All four current binding alternatives
are supported. Numeric codes follow the shared afterhours key/gamepad codes.

Decoding builds a separate complete list. Apply it to the active mapping only
after successful decoding, so invalid input does not partially replace live
bindings. Action names, layers, filenames, file containers and migrations from
old application-specific formats remain caller-owned. This header adds no JSON
dependency and is independent of binding display.

`v1` identifies the library's binding wire format, independently of the app's
settings schema version. It is not configurable: a decoder version must agree
with the bytes it accepts. Apps may keep their own schema version around these
strings or serialize the public `AnyInput` alternatives using a different codec.
Using afterhours input does not require using this persistence format.
