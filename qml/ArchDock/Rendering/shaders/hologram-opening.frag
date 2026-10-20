#version 440
// A projector's opening, the lens the light leaves: a glowing disc seen in
// perspective (the item is the ellipse's box with room for its glow, the
// lens filling `lens` of it), lighter toward its far side, a white core in
// its middle, soft light round it; a projector's plate rings it with metal
// (`ring`). Its colour is `tint`, or where `own` says the icon picture's
// main colour; it breathes a little with `time`.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 tint;
    float time;
    float lens;
    float ring;
    float own;
    float strength;
};
layout(binding = 1) uniform sampler2D source;

vec3 mainColour()
{
    vec4 sum = vec4(0.0);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            sum += texture(source, vec2(0.2 + 0.2 * float(i), 0.2 + 0.2 * float(j)));
    vec3 average = sum.a > 1e-3 ? sum.rgb / sum.a : vec3(1.0);
    return average / max(max(average.r, average.g), max(average.b, 0.2));
}

void main()
{
    vec2 p = (qt_TexCoord0 - vec2(0.5)) * 2.0 / lens;
    float r = length(p);
    vec3 colour = mix(tint.rgb, mainColour(), own);
    float breathe = 0.92 + 0.08 * sin(time * 2.1);
    // The lens: lighter toward its far side (up the item), a white core.
    float disc = 1.0 - smoothstep(0.94, 1.0, r);
    vec3 lensColour = colour * mix(1.45, 0.67, clamp(qt_TexCoord0.y * 1.2 - 0.1, 0.0, 1.0));
    lensColour = mix(lensColour, vec3(1.0), 0.8 * (1.0 - smoothstep(0.32, 0.55, r)));
    // Soft light round it.
    float glow = exp(-max(0.0, r - 1.0) * 5.0) * (1.0 - disc) * 0.55;
    vec4 lit = vec4(lensColour * disc + colour * glow, disc * 0.95 + glow * 0.6) * breathe * strength;
    // The metal ring of a projector's plate, solid, round the lens.
    float metal = ring * smoothstep(0.98, 1.0, r) * (1.0 - smoothstep(1.14, 1.17, r));
    vec3 steel = mix(vec3(0.27, 0.31, 0.34), vec3(0.82, 0.85, 0.88), clamp(1.0 - qt_TexCoord0.y, 0.0, 1.0));
    vec4 result = mix(lit, vec4(steel, 1.0), metal);
    fragColor = result * qt_Opacity;
}
