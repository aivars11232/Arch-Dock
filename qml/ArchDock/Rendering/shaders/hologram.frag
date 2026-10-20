#version 440
// A picture drawn as a hologram: its own shapes as light, see-through, its
// outline (where the picture ends) and its inline (where its brightness
// changes, the symbol's inner lines) brighter, crossed by drifting scan
// lines, blinking now and then as a weak signal does. Each of the three
// parts is drawn in its colour, or in the picture's own colour made light
// where `own` says (x outline, y inline, z volume). After the holographic
// material for three.js by Anderson Mancini (MIT) and the Godot "SciFi
// Hologram" shader (MIT): scan lines, a signal's blink, an edge glow.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 outlineColour;
    vec4 inlineColour;
    vec4 volumeColour;
    vec4 own;
    vec2 texel;
    float time;
    float lines;
    float flicker;
    float edges;
    float seed;
    float body;
    vec4 area;
};
layout(binding = 1) uniform sampler2D source;

// Where a point of this item lies in the source's picture, which can reach
// past the item (a blur's padding round a layer): offset, then scale.
vec2 at(vec2 uv) { return area.xy + uv * area.zw; }

float hash(float n) { return fract(sin(n * 12.9898 + seed * 78.233) * 43758.5453); }
float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main()
{
    vec4 c = texture(source, at(qt_TexCoord0));
    float a = c.a;
    vec3 colour = c.rgb / max(a, 1e-3);
    float lum = luminance(colour);
    // The outline: how much less of the picture lies round the pixel; the
    // inline: how much its brightness differs from the round's.
    float around = 0.0;
    float lumAround = 0.0;
    for (int k = 0; k < 8; ++k) {
        float t = float(k) * 0.785398;
        vec2 d = vec2(cos(t), sin(t));
        around += texture(source, at(qt_TexCoord0 + d * texel * 2.5)).a;
        vec4 o = texture(source, at(qt_TexCoord0 + d * texel * 1.5));
        lumAround += luminance(o.rgb / max(o.a, 1e-3)) * o.a + lum * (1.0 - o.a);
    }
    float outer = clamp((a - around / 8.0) * 3.0, 0.0, 1.0);
    float inner = clamp(abs(lum - lumAround / 8.0) * 4.0 * a, 0.0, 1.0);
    // The picture's own colour as light, and each part's colour.
    vec3 lit = colour * 1.1 + vec3(0.08);
    vec3 volume = mix(volumeColour.rgb * (0.75 + 0.55 * lum), lit, own.z);
    vec3 innerLine = mix(inlineColour.rgb, mix(lit, vec3(1.0), 0.45), own.y);
    vec3 outerLine = mix(outlineColour.rgb, mix(lit, vec3(1.0), 0.65), own.x);
    vec3 result = mix(volume, innerLine, clamp(inner * edges * 1.2, 0.0, 1.0));
    result = mix(result, outerLine, clamp(outer * edges * 1.2, 0.0, 1.0));
    float scan = 0.7 + 0.3 * sin((qt_TexCoord0.y * lines + time * 1.6) * 6.2831853);
    float blink = 1.0 - min(1.0, flicker) * 0.55 * step(1.0 - 0.06 * flicker, hash(floor(time * 14.0)));
    float alpha = a * (0.55 * body + 0.45 * max(outer, inner)) * blink;
    fragColor = vec4(result * scan * 1.15 * alpha, alpha) * qt_Opacity;
}
