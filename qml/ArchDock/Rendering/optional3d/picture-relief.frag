// The picture in its own colours over its real relief (picture-relief.vert
// moves the mesh): its finest grain, finer than the mesh, only bends the
// light; it is shadowed where its relief rises between it and the key
// light, its low parts a little darker, as light reaches them less, with a
// faint polish over it. How the picture lies on the surface is read from
// how its coordinates change from pixel to pixel, so this works on any
// mesh, a theme's own as well.
VARYING vec2 reliefUv;
VARYING float reliefRise;
VARYING float reliefHeight;
VARYING vec3 reliefWorld;
VARYING float reliefWorldDepth;

// The relief's height: the picture's brightness (or darkness), sampled one
// halving blurrier than it is drawn, its raised parts all at the top.
float reliefLuma(vec2 uv, vec2 dx, vec2 dy)
{
    float blur = 2.0 + reliefSmoothing * 6.0;
    float luma = dot(textureGrad(pictureMap, uv, dx * blur, dy * blur).rgb, vec3(0.299, 0.587, 0.114));
    float span = max(0.02, 1.0 - reliefContrast);
    return smoothstep(reliefLevel - span * 0.5, reliefLevel + span * 0.5, mix(luma, 1.0 - luma, reliefInvert));
}

void MAIN()
{
    vec2 uv = reliefUv;
    vec2 dx = dFdx(uv);
    vec2 dy = dFdy(uv);
    vec3 n = normalize(NORMAL);
    // How the picture's u and v grow per unit of the scene along the
    // surface here.
    vec3 px = dFdx(reliefWorld);
    vec3 py = dFdy(reliefWorld);
    float area = dot(n, cross(px, py));
    vec3 gradU = vec3(0.0);
    vec3 gradV = vec3(0.0);
    if (abs(area) > 1e-14) {
        vec3 alongY = cross(py, n);
        vec3 alongX = cross(n, px);
        gradU = (alongY * dx.x + alongX * dy.x) / area;
        gradV = (alongY * dx.y + alongX * dy.y) / area;
    }
    float depth = reliefWorldDepth;
    bool shaped = depth > 0.0 && abs(area) > 1e-14;

    // The grain's slope bends the light, over two of the picture's points,
    // and never more than so far.
    vec2 texel = 2.0 / vec2(textureSize(pictureMap, 0));
    float hu = (reliefLuma(uv + vec2(texel.x, 0.0), dx, dy) - reliefLuma(uv - vec2(texel.x, 0.0), dx, dy))
        / (2.0 * texel.x);
    float hv = (reliefLuma(uv + vec2(0.0, texel.y), dx, dy) - reliefLuma(uv - vec2(0.0, texel.y), dx, dy))
        / (2.0 * texel.y);
    if (shaped) {
        vec3 lean = (gradU * hu + gradV * hv) * depth;
        float steep = length(lean);
        NORMAL = normalize(n - lean * min(1.0, 1.2 / max(steep, 1e-6)));
    }

    // Shadowed where the relief rises between here and the key light.
    float height = reliefLuma(uv, dx, dy);
    float lit = 1.0;
    vec3 light = normalize(keyLightDirection);
    float toward = dot(light, n);
    if (shaped && toward > 0.02) {
        vec3 up = (light - n * toward) / toward * depth;
        vec2 step = vec2(dot(gradU, up), dot(gradV, up)) / 16.0;
        float blocked = 0.0;
        for (int i = 1; i <= 16; ++i) {
            float climb = float(i) / 16.0;
            float h = reliefLuma(uv + step * float(i), dx, dy);
            blocked = max(blocked, (h - height - climb) * (1.0 - climb));
        }
        lit = 1.0 - clamp(blocked * 5.0, 0.0, 1.0) * reliefShade * 0.6;
    }

    // The picture's sRGB colours made linear, as built-in materials do;
    // its low parts a little darker.
    vec3 c = textureGrad(pictureMap, uv, dx, dy).rgb;
    c = c * (c * (c * 0.305306011 + vec3(0.682171111)) + vec3(0.012522878));
    float hollow = shaped ? mix(1.0, 0.55 + 0.45 * smoothstep(0.0, 0.6, height), reliefShade) : 1.0;
    BASE_COLOR = vec4(c * hollow * lit, 1.0);
    ROUGHNESS = surfaceRoughness;
    METALNESS = 0.0;
    // A faint polish over it, as on worn stone.
    CLEARCOAT_AMOUNT = 0.2;
    CLEARCOAT_ROUGHNESS = 0.35;
}
