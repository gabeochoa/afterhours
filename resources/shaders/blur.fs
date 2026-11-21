#version 330

// Separable gaussian for effects::BlurPass: one pass along `direction`
// (in UV units), run twice (horizontal, then vertical). Weights match the
// sokol/Metal blur in drawing_helpers.h so both backends blur identically;
// direction (0, 0) is an exact copy.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec2 direction;

out vec4 finalColor;

vec4 tap(float offset) {
    // Render textures wrap on raylib; clamp like sokol's CLAMP_TO_EDGE sampler.
    return texture(texture0, clamp(fragTexCoord + direction * offset, 0.0, 1.0));
}

void main() {
    vec4 sum = tap(0.0) * 0.2270270270;
    sum += tap(1.3846153846) * 0.3162162162;
    sum += tap(-1.3846153846) * 0.3162162162;
    sum += tap(3.2307692308) * 0.0702702703;
    sum += tap(-3.2307692308) * 0.0702702703;
    finalColor = sum * colDiffuse * fragColor;
}
