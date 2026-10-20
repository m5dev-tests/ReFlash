#version 300 es
precision highp float;
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uDst;      // Original unblurred object texture
uniform sampler2D uSrc;      // Blurred alpha/color pass texture
uniform vec2 uOffset;        // Shadow/Glow offset: (-cos(angle)*dist/W, -sin(angle)*dist/H)
uniform float uStrength;     // Filter strength multiplier
uniform vec4 uColor;         // Premultiplied filter color: (R*A, G*A, B*A, A)
uniform vec4 uHighlight;     // Bevel highlight color (or 0 for glow/shadow)

// Compile-time variant flags
uniform bool uIsInner;
uniform bool uIsKnockout;
uniform bool uIsHideObject;
uniform bool uIsFull;        // Bevel OnTop

vec4 applyStrength(float alpha, vec4 col) {
    return col * clamp(alpha * uStrength, 0.0, col.a);
}

vec4 compositeShadow(vec4 dstPx, vec4 shadowPx) {
    float sa = shadowPx.a;
    float da = dstPx.a;
    if (da > 0.0) {
        if (uIsInner || uIsFull) {
            if (uIsKnockout) return shadowPx;
            if (uIsHideObject) return (sa > 0.0) ? shadowPx : uColor * (1.0 - da);
            if (sa > 0.0) {
                float outA = max(da, sa);
                return vec4(shadowPx.rgb + dstPx.rgb * (1.0 - sa), outA);
            }
        } else {
            if (uIsKnockout) {
                float outA = (1.0 - da) * sa;
                return (sa > 0.0) ? vec4(shadowPx.rgb * outA, outA) : vec4(0.0);
            }
            if (uIsHideObject) return shadowPx;
            if (sa > 0.0) {
                float outA = 1.0 - (1.0 - sa) * (1.0 - da);
                return vec4(dstPx.rgb + shadowPx.rgb * (1.0 - da), outA);
            }
        }
    } else if (sa > 0.0) {
        if (!uIsInner || uIsFull) return shadowPx;
    }
    return dstPx;
}

void main() {
    vec4 dstPx = texture(uDst, vUV);
    vec2 srcUV = vUV + uOffset;
    vec4 shadowPx = vec4(0.0);

    if (clamp(srcUV, vec2(0.0), vec2(1.0)) == srcUV) {
        float srcAlpha = texture(uSrc, srcUV).a;
        shadowPx = (!uIsInner) ? uColor * srcAlpha : uColor * dstPx.a * (1.0 - srcAlpha);
    }

    shadowPx = applyStrength(shadowPx.a, uColor);
    fragColor = compositeShadow(dstPx, shadowPx);
}
