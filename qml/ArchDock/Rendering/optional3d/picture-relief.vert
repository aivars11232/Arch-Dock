// The picture's brightness is the surface's height, as real geometry: the
// mesh itself moves. On a mesh cut for it (uv1's x is how far each point
// may move) each point moves along its normal, by up to reliefDepth:
// inward, the picture's raised parts (a stone's face) staying at the band's
// own surface and the rest (the gaps) sinking in, as stones pressed against
// a gabion's mesh; or outward, the raised parts rising out of the band, as
// little mountains do. Which parts are raised: the light ones, or with
// reliefInvert the dark ones. The movement follows the picture blurred to
// about two of its points per step of the mesh (meshStep), which the mesh
// can follow; on a band made for it its normal leans with the slope.
VARYING vec2 reliefUv;
VARYING float reliefRise;
VARYING float reliefHeight;
VARYING vec3 reliefWorld;
VARYING float reliefWorldDepth;

// The picture's coordinates at a point: the band's own, repeated along it,
// or on a mesh not made for it (a theme's own) laid on each face straight
// from the side it faces (uv1's y, chosen for the whole face when the mesh
// was cut, so no face is torn between two sides), boxScale repeats per
// unit, so its stones are as large on its walls as on its top.
vec2 reliefPictureUv(vec2 meshUv, vec3 position, float side)
{
    if (boxScale <= 0.0)
        return vec2(meshUv.x * repeatU, meshUv.y);
    return (side < 0.5 ? position.xy : side < 1.5 ? position.yz : position.xz) * boxScale;
}

// The height a brightness stands for: half height at reliefLevel, the
// climb through it as steep as reliefContrast says.
float reliefHeightAt(vec2 uv, float lod)
{
    float luma = dot(textureLod(pictureMap, uv, lod).rgb, vec3(0.299, 0.587, 0.114));
    float span = max(0.02, 1.0 - reliefContrast);
    return smoothstep(reliefLevel - span * 0.5, reliefLevel + span * 0.5, mix(luma, 1.0 - luma, reliefInvert));
}

void MAIN()
{
    vec2 uv = reliefPictureUv(UV0, VERTEX, UV1.y);
    reliefUv = uv;
    reliefRise = UV1.x;
    reliefHeight = 1.0;
    reliefWorldDepth = reliefDepth * length((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
    if (UV1.x > 0.0) {
        vec2 size = vec2(textureSize(pictureMap, 0));
        // The mesh's units one width of the picture spans, and its steps
        // across that width.
        float span = boxScale > 0.0 ? 1.0 / boxScale : lengthU / max(repeatU, 1.0);
        float steps = span / max(meshStep, 1e-4);
        float level = max(0.0, log2(size.x / max(2.0 * steps, 1.0))) + reliefSmoothing * 3.0;
        float h = reliefHeightAt(uv, level);
        float amount = reliefDepth * UV1.x;
        reliefHeight = h;
        VERTEX += NORMAL * (h - 1.0 + reliefOutward) * amount;
        if (dot(TANGENT, TANGENT) > 0.0 && dot(BINORMAL, BINORMAL) > 0.0) {
            vec2 texel = exp2(level) / size;
            float slopeU = (reliefHeightAt(uv + vec2(texel.x, 0.0), level) - reliefHeightAt(uv - vec2(texel.x, 0.0), level))
                / (2.0 * texel.x) * repeatU / lengthU;
            float slopeV = (reliefHeightAt(uv + vec2(0.0, texel.y), level) - reliefHeightAt(uv - vec2(0.0, texel.y), level))
                / (2.0 * texel.y) / lengthV;
            NORMAL = normalize(NORMAL - (TANGENT * slopeU + BINORMAL * slopeV) * amount);
        }
    }
    reliefWorld = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
