#version 300 es

precision highp float;

in vec2 v_texCoord;
out vec4 fragColor;

uniform sampler2D u_srcTexture;
uniform sampler2D u_dstTexture;

// Blend Mode Identifier:
// 0: Normal, 2: Multiply, 3: Screen, 6: Difference/Invert, 7: Add
uniform int u_blendMode;

// ColorTransform uniforms
uniform vec4 u_colorMult; // (rMult, gMult, bMult, aMult)
uniform vec4 u_colorAdd;  // (rAdd, gAdd, bAdd, aAdd)

vec3 blendAdd(vec3 src, vec3 dst) {
    return min(src + dst, vec3(1.0));
}

vec3 blendMultiply(vec3 src, vec3 dst) {
    return src * dst;
}

vec3 blendScreen(vec3 src, vec3 dst) {
    return vec3(1.0) - (vec3(1.0) - src) * (vec3(1.0) - dst);
}

vec3 blendDifference(vec3 src, vec3 dst) {
    return abs(dst - src);
}

void main() {
    vec4 srcColor = texture(u_srcTexture, v_texCoord);
    vec4 dstColor = texture(u_dstTexture, v_texCoord);

    // Apply Flash ColorTransform matrix multipliers and offsets to source color
    srcColor = clamp(srcColor * u_colorMult + u_colorAdd, 0.0, 1.0);

    vec3 blendedRgb = srcColor.rgb;

    switch (u_blendMode) {
        case 2: // Multiply
            blendedRgb = blendMultiply(srcColor.rgb, dstColor.rgb);
            break;
        case 3: // Screen
            blendedRgb = blendScreen(srcColor.rgb, dstColor.rgb);
            break;
        case 6: // Difference / Invert
        case 9: // Invert
            blendedRgb = blendDifference(srcColor.rgb, dstColor.rgb);
            break;
        case 7: // Add
            blendedRgb = blendAdd(srcColor.rgb, dstColor.rgb);
            break;
        case 0: // Normal
        default:
            blendedRgb = mix(dstColor.rgb, srcColor.rgb, srcColor.a);
            fragColor = vec4(blendedRgb, srcColor.a + dstColor.a * (1.0 - srcColor.a));
            return;
    }

    // Blend result compositing with destination alpha according to source alpha
    vec3 finalRgb = mix(dstColor.rgb, blendedRgb, srcColor.a);
    float finalAlpha = clamp(srcColor.a + dstColor.a * (1.0 - srcColor.a), 0.0, 1.0);

    fragColor = vec4(finalRgb, finalAlpha);
}
