// A projector's opening, the lens the light leaves, lying flat: a glowing
// disc filling `lens` of its square, a white core in its middle, soft light
// round it, breathing a little with `time`; in `tint`, or where `own` says
// the icon picture's main colour. Added to the scene.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;

vec3 mainColour()
{
    vec4 sum = vec4(0.0);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            sum += texture(picture, vec2(0.2 + 0.2 * float(i), 0.2 + 0.2 * float(j)));
    vec3 average = sum.a > 1e-3 ? sum.rgb / sum.a : vec3(1.0);
    return average / max(max(average.r, average.g), max(average.b, 0.2));
}

void MAIN()
{
    vec2 p = (pictureUv - vec2(0.5)) * 2.0 / lens;
    float r = length(p);
    vec3 colour = mix(tint.rgb, mainColour(), own);
    float breathe = 0.92 + 0.08 * sin(time * 2.1);
    float disc = 1.0 - smoothstep(0.94, 1.0, r);
    // Added light: the lens lit from within, not a lamp - a white core and
    // a little light round it, so the scene's glow does not burn it white.
    vec3 lensColour = mix(colour * mix(0.75, 0.4, smoothstep(0.0, 1.0, r)), vec3(1.0),
                          0.45 * (1.0 - smoothstep(0.22, 0.45, r)));
    float glow = exp(-max(0.0, r - 1.0) * 6.0) * (1.0 - disc) * 0.25;
    FRAGCOLOR = vec4((lensColour * disc + colour * glow) * breathe * strength * brightness, 0.0);
}
