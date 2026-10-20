// A surface drawn as light, as the flat holograms are (shaders/hologram.frag):
// its picture's own shapes, see-through, its outline - where the solid turns
// away from the eye, and where the picture ends - and its inline - where
// the picture's brightness changes - brighter, each part in its colour or
// in the picture's own made light (`own`), crossed by scan lines across the
// view and blinking now and then as a weak signal does. After the
// holographic material for three.js by Anderson Mancini (MIT): a Fresnel
// rim, screen-space scan lines, a signal's blink, added light.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;

float hash(float n) { return fract(sin(n * 12.9898 + seed * 78.233) * 43758.5453); }
float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void MAIN()
{
    vec4 c = usePicture > 0.5 ? texture(picture, pictureUv) : vec4(ownColour.rgb, 1.0);
    float a = c.a;
    if (a < 0.03)
        discard;
    vec3 colour = c.rgb / max(a, 1e-3);
    // A dark picture's own colours kept: lifted to a brighter light of the
    // same colour.
    float peak = max(colour.r, max(colour.g, colour.b));
    colour = mix(colour, colour / max(peak, 0.04) * clamp(0.3 + 1.4 * peak, 0.0, 1.0), keepColour);
    float lum = luminance(colour);
    vec3 toEye = normalize(CAMERA_POSITION - worldPoint);
    // Kept colours have a narrower rim, so a rounded solid is not edged
    // all over.
    float rim = pow(1.0 - abs(dot(normalize(worldNormal), toEye)), 2.0 + 2.0 * keepColour);
    float inner = 0.0;
    float edge = 0.0;
    if (usePicture > 0.5) {
        inner = clamp(length(vec2(dFdx(lum), dFdy(lum))) * 6.0, 0.0, 1.0);
        edge = clamp(length(vec2(dFdx(a), dFdy(a))) * 4.0, 0.0, 1.0);
    }
    float outer = max(rim, edge);
    vec3 lit = colour * 1.1 + vec3(0.08 * (1.0 - keepColour));
    float whiten = 1.0 - 0.6 * keepColour;
    vec3 volume = mix(volumeColour.rgb * (0.75 + 0.55 * lum), lit, own.z);
    vec3 innerLine = mix(inlineColour.rgb, mix(lit, vec3(1.0), 0.45 * whiten), own.y);
    vec3 outerLine = mix(outlineColour.rgb, mix(lit, vec3(1.0), 0.65 * whiten), own.x);
    vec3 result = mix(volume, innerLine, clamp(inner * 0.96, 0.0, 1.0));
    result = mix(result, outerLine, clamp(outer * 1.2, 0.0, 1.0));
    float scan = 0.7 + 0.3 * sin((FRAGCOORD.y / max(1.0, lineSpacing) + time * 1.6) * 6.2831853);
    float blink = 1.0 - min(1.0, flicker) * 0.55 * step(1.0 - 0.06 * flicker, hash(floor(time * 14.0)));
    float alpha = a * (0.55 * body + 0.45 * max(outer, inner)) * blink;
    FRAGCOLOR = vec4(result * scan * 1.15 * alpha * brightness, 0.0);
}
