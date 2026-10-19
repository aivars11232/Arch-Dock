// The surface flowing along the band: its relief map in two layers at
// different speeds bending the light. Water: the bed stands still and
// wobbles with the ripples passing over it (refraction), the sky is
// mirrored where the surface tilts toward the viewer (Schlick's
// approximation of Fresnel), the icons' mirror images lie on it where the
// mirrored camera drew them, and the lights glitter on it. Lava: the crust
// and the lava run along together, the crust rough and lit, the lava
// glowing with its own light in a slow shimmer, a second layer of lava
// showing dimly through the crust.
VARYING vec2 flowUv;
VARYING vec3 flowTangent;
VARYING vec3 flowBinormal;

vec3 toLinear(vec3 c)
{
    // sRGB made linear as the built-in materials do.
    return c * (c * (c * 0.305306011 + vec3(0.682171111)) + vec3(0.012522878));
}

void MAIN()
{
    vec2 flow1 = vec2(flowUv.x + flowOffset, flowUv.y);
    vec2 flow2 = vec2(flowUv.x * 1.7 - flowOffset * 0.6, flowUv.y * 1.7 + flowOffset * 0.2);
    vec3 r1 = texture(reliefMap, flow1).xyz * 2.0 - 1.0;
    vec3 r2 = texture(reliefMap, flow2).xyz * 2.0 - 1.0;
    vec2 bend = (r1.xy + r2.xy) * 0.5;
    vec3 n = normalize(NORMAL);
    vec3 t = normalize(flowTangent);
    vec3 b = normalize(flowBinormal);
    NORMAL = normalize(n + (t * bend.x + b * bend.y) * 0.6);
    vec3 view = normalize(CAMERA_POSITION - VAR_WORLD_POSITION);
    if (molten) {
        // How much lava a texel is: the crust is grey, the lava orange, so
        // red over blue tells them apart.
        vec3 c = texture(surfaceMap, flow1).rgb;
        vec3 under = texture(surfaceMap, flow2).rgb;
        float heat = smoothstep(0.06, 0.35, c.r - c.b);
        float heatUnder = smoothstep(0.06, 0.35, under.r - under.b);
        c = mix(c, under, (1.0 - heat) * heatUnder * 0.5);
        heat = max(heat, heatUnder * (1.0 - heat) * 0.5);
        // Two shimmers per loop, so the light never jumps when the loop
        // starts over.
        float shimmer = 0.5 + 0.5 * sin(flowOffset * 12.566371 + flowUv.x * 12.0 + flowUv.y * 3.0);
        vec3 lava = toLinear(c);
        BASE_COLOR = vec4(lava, 1.0);
        EMISSIVE_COLOR = lava * heat * (1.3 + 0.6 * shimmer);
        ROUGHNESS = mix(0.85, 0.3, heat);
        METALNESS = 0.0;
        SPECULAR_AMOUNT = 0.4;
    } else {
        // The bed stands still; the ripples passing over it bend what is
        // seen of it.
        vec3 water = toLinear(texture(surfaceMap, flowUv + bend * 0.012).rgb);
        // Shallow at the banks: the water is lighter there and the bed shows
        // more, deep in the middle of the band.
        float bank = 1.0 - sin(3.1415927 * clamp(flowUv.y, 0.0, 1.0));
        water *= 1.0 + 0.8 * bank * bank;
        float facing = clamp(dot(view, NORMAL), 0.0, 1.0);
        float fresnel = 0.02 + 0.98 * pow(1.0 - facing, 5.0);
        vec3 reflected = reflect(-view, NORMAL);
        // The sky is far brighter than the bed, so its mirror image shows
        // even where Fresnel keeps the share small.
        vec3 sky = 2.2 * mix(vec3(0.55, 0.62, 0.70), vec3(0.30, 0.48, 0.80), clamp(reflected.y, 0.0, 1.0));
        if (mirrored) {
            // The mirror images' sheet lies over the view: sample it at this
            // point's own place on the screen, from its projection (the
            // fragment's pixel coordinates run past the view's size when the
            // scene is supersampled), the sheet's rows running from the top,
            // wobbled by the ripples.
            vec4 clip = VIEWPROJECTION_MATRIX * vec4(VAR_WORLD_POSITION, 1.0);
            vec2 screen = clip.xy / clip.w * 0.5 + 0.5;
            if (NDC_Y_UP < 0.0) screen.y = 1.0 - screen.y;
            vec4 mirror = texture(reflectionMap, vec2(screen.x, 1.0 - screen.y) + bend * 0.015);
            // Fainter and darker than the icon itself, as a reflection in
            // water is.
            water = water * (1.0 - mirror.a * mirrorStrength) + mirror.rgb * 0.7 * mirrorStrength;
        }
        BASE_COLOR = vec4(mix(water, sky * 0.9, fresnel), 1.0);
        EMISSIVE_COLOR = sky * fresnel * 0.25;
        ROUGHNESS = 0.05;
        METALNESS = 0.0;
        SPECULAR_AMOUNT = 1.0;
    }
}
