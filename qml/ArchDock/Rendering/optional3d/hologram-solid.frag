// A 3D icon drawn as a hologram: its solid made of light, so that its shape
// shows, not its picture alone. As every hologram (hologram-surface.frag):
// the picture's own shapes, see-through, its outline - where the solid
// turns away from the eye, and where the picture ends - and its inline -
// where the picture's brightness changes - brighter, each in its colour or
// in the picture's own made light (`own`), blinking now and then as a weak
// signal does. A solid adds three things: its far side shows through its
// near side, fainter and mostly its outline; a light from above the eye
// models its rounded parts; and the scan lines are planes across the solid
// in its own frame, leaning as a projector's level lines seen from a little
// above, so they bend over what rises toward the eye and run on round its
// sides and its far side, drifting up.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;
VARYING vec3 shapePoint;

float hash(float n) { return fract(sin(n * 12.9898 + seed * 78.233) * 43758.5453); }
float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

// The picture's brightness and opacity a step to either side, along and
// across it: where they change are its inner lines and its edge, read from
// the picture itself, so they lie on the solid as clean as on the picture
// however its surface turns (read from the screen, a rounded surface's
// shading broke them into specks).
vec2 pictureChange(vec2 uv)
{
    const float aside = 0.01;
    vec4 right = texture(picture, uv + vec2(aside, 0.0));
    vec4 left = texture(picture, uv - vec2(aside, 0.0));
    vec4 above = texture(picture, uv + vec2(0.0, aside));
    vec4 below = texture(picture, uv - vec2(0.0, aside));
    float lumX = luminance(right.rgb) - luminance(left.rgb);
    float lumY = luminance(above.rgb) - luminance(below.rgb);
    return vec2(length(vec2(lumX, lumY)), length(vec2(right.a - left.a, above.a - below.a)));
}

void MAIN()
{
    vec4 c = texture(picture, pictureUv);
    float a = c.a;
    if (a < 0.03)
        discard;
    vec3 colour = c.rgb / max(a, 1e-3);
    float lum = luminance(colour);
    vec3 toEye = normalize(CAMERA_POSITION - worldPoint);
    vec3 normal = normalize(worldNormal);
    float facing = dot(normal, toEye);
    // The far side, seen through the near one, faces away from the eye:
    // only its outline shows there, never its inner lines over the near
    // side's.
    float farSide = facing < 0.0 ? 1.0 : 0.0;
    vec3 seen = facing < 0.0 ? -normal : normal;
    float rim = pow(1.0 - abs(facing), 2.0);
    vec2 change = pictureChange(pictureUv);
    float inner = clamp(change.x * 2.0, 0.0, 1.0) * (1.0 - farSide);
    float edge = clamp(change.y * 1.5, 0.0, 1.0);
    float outer = max(rim, edge);
    // The light: from above the eye, a little to its left.
    vec3 forward = normalize(CAMERA_DIRECTION);
    vec3 right = cross(forward, vec3(0.0, 1.0, 0.0));
    right = length(right) > 1e-3 ? normalize(right) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(right, forward);
    vec3 light = normalize(-forward * 0.75 + up * 0.6 - right * 0.35);
    float shade = 0.45 + 0.75 * max(dot(seen, light), 0.0);
    vec3 lit = colour * 1.1 + vec3(0.08);
    vec3 volume = mix(volumeColour.rgb * (0.75 + 0.55 * lum), lit, own.z) * shade;
    vec3 innerLine = mix(inlineColour.rgb, mix(lit, vec3(1.0), 0.45), own.y);
    vec3 outerLine = mix(outlineColour.rgb, mix(lit, vec3(1.0), 0.65), own.x);
    vec3 result = mix(volume, innerLine, clamp(inner * 0.96, 0.0, 1.0));
    result = mix(result, outerLine, clamp(outer * 1.2, 0.0, 1.0));
    // The level lines, thin and bright, as wide on the screen wherever they
    // lie; between them the body, a little less bright.
    float level = (shapePoint.y + 0.55 * shapePoint.z) * lineCount / 100.0 - time * 0.35;
    float apart = abs(fract(level + 0.5) - 0.5);
    float line = 1.0 - smoothstep(0.07, 0.07 + max(fwidth(level), 0.03), apart);
    float scan = 0.68 + 0.6 * line;
    float blink = 1.0 - min(1.0, flicker) * 0.55 * step(1.0 - 0.06 * flicker, hash(floor(time * 14.0)));
    float body = mix(0.5, 0.12, farSide);
    float lineShare = mix(0.5, 0.55, farSide);
    float alpha = a * (body + lineShare * max(outer, inner)) * blink;
    FRAGCOLOR = vec4(result * scan * 1.15 * alpha * brightness, 0.0);
}
