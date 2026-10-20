#version 300 es
precision highp float;
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTexture;
uniform float uBlur;       // blur dimension in texels
uniform float uInvBlur;    // 1.0 / uBlur
uniform vec2 uDirection;   // (1.0/width, 0.0) or (0.0, 1.0/height)

void main() {
    if (uBlur <= 1.0) {
        fragColor = texture(uTexture, vUV);
        return;
    }
    float halfBlur = max(0.0, (uBlur - 1.0) * 0.5);
    float radius = ceil(halfBlur);
    float fracEdge = 1.0 + halfBlur - radius;
    vec2 offset = uDirection * radius;
    vec4 total = texture(uTexture, vUV);
    float weight = fracEdge;

    // Accumulate outer taps
    for (int i = 0; i < 255; i++) {
        if (float(i) >= radius) break;
        total += (texture(uTexture, vUV - offset) + texture(uTexture, vUV + offset)) * weight;
        offset -= uDirection;
        weight = 1.0;
    }
    fragColor = total * uInvBlur;
}
