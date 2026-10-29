// A 3D icon made into an object, drawn as a hologram: as a solid's
// hologram (hologram-solid.frag), its body see-through and lit from above
// the eye, its far side showing faintly through, the scan lines planes
// across it drifting up, blinking now and then; but its colours are the
// object's own at each point, not a picture's laid over it from the front.
// Its inner lines lie where those colours change from one screen pixel to
// the next, so they stay as fine as the screen at any size; its rim is
// narrower and its scan lines finer.
VARYING vec3 pointColour;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;
VARYING vec3 shapePoint;

float hash(float n) { return fract(sin(n * 12.9898 + seed * 78.233) * 43758.5453); }
float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void MAIN()
{
    vec3 colour = pointColour;
    float lum = luminance(colour);
    vec3 toEye = normalize(CAMERA_POSITION - worldPoint);
    vec3 normal = normalize(worldNormal);
    float facing = dot(normal, toEye);
    // The far side, seen through the near one, shows its outline only.
    float farSide = facing < 0.0 ? 1.0 : 0.0;
    vec3 seen = facing < 0.0 ? -normal : normal;
    float rim = pow(1.0 - abs(facing), 3.0);
    float change = length(vec2(dFdx(lum), dFdy(lum)));
    float inner = clamp(change * 7.0, 0.0, 1.0) * (1.0 - farSide);
    float outer = rim;
    // The light: from above the eye, a little to its left.
    vec3 forward = normalize(CAMERA_DIRECTION);
    vec3 right = cross(forward, vec3(0.0, 1.0, 0.0));
    right = length(right) > 1e-3 ? normalize(right) : vec3(1.0, 0.0, 0.0);
    vec3 up = cross(right, forward);
    vec3 light = normalize(-forward * 0.75 + up * 0.6 - right * 0.35);
    float shade = 0.45 + 0.75 * max(dot(seen, light), 0.0);
    vec3 lit = colour * 1.1 + vec3(0.08);
    vec3 volume = mix(volumeColour.rgb * (0.55 + 0.95 * lum), lit, own.z) * shade;
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
    float alpha = (body + lineShare * max(outer, inner)) * blink;
    FRAGCOLOR = vec4(result * scan * 1.15 * alpha * brightness, 0.0);
}
