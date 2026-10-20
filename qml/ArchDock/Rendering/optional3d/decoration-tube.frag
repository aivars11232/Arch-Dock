// A decoration's tubes lit by the scene: the bark in its own colours, its
// grain finer than the mesh bending the light by bump mapping from the
// heights' slopes on the screen, with no tangents needed (Mikkelsen, "Bump
// Mapping Unparametrized Surfaces on the GPU", 2010), and its cracks a little
// darker as the light reaches them less; a cut end shows the design's face,
// a mark on the bark (a knot hole) its own colour.
VARYING vec2 barkUv;
VARYING float kind;
VARYING float markBlue;
VARYING vec3 worldPoint;

// sRGB colours made linear, as built-in materials do.
vec3 linearColour(vec3 c)
{
    return c * (c * (c * 0.305306011 + vec3(0.682171111)) + vec3(0.012522878));
}

void MAIN()
{
    bool mark = kind > 2.5;
    bool cut = kind > 1.5 && !mark;
    // Both pictures read and every slope taken for every pixel, so the
    // edge where the bark meets a cut end is read as cleanly as the rest.
    vec3 bark = texture(barkMap, barkUv).rgb;
    vec3 face = texture(capMap, barkUv).rgb;
    vec3 colour = mark ? vec3(barkUv, markBlue) : cut ? face : bark;
    float height = texture(reliefMap, barkUv).r;
    float h = height * reliefDepth;
    vec3 across = dFdx(worldPoint);
    vec3 down = dFdy(worldPoint);
    float hx = dFdx(h);
    float hy = dFdy(h);
    vec3 n = normalize(NORMAL);
    if (!cut && !mark && hasRelief > 0.5) {
        vec3 r1 = cross(down, n);
        vec3 r2 = cross(n, across);
        float area = dot(across, r1);
        vec3 slope = sign(area) * (hx * r1 + hy * r2);
        n = normalize(abs(area) * n - slope);
        NORMAL = n;
    }
    float crevice = cut || mark || hasRelief < 0.5 ? 1.0 : 0.6 + 0.4 * smoothstep(0.1, 0.7, height);
    BASE_COLOR = vec4(linearColour(colour) * crevice, 1.0);
    ROUGHNESS = clamp(roughness, 0.04, 1.0);
    METALNESS = metalness;
    SPECULAR_AMOUNT = 0.3;
    OCCLUSION_AMOUNT = crevice;
}
