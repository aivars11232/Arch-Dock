// A panel decoration's solid, lit as its flat pictures were (the app's
// DecorationLighting) but by the 3D scene as it turns: the scene's lights
// shade it through its colour, metalness, roughness and coat; the relief's
// grain, finer than the mesh, bends the light over its upper surface; the
// relief casts its own shadows away from the key light; the studio its
// flat pictures were lit in is mirrored in it as it turns (a softbox
// overhead, a strip light at the left, a dark room, a little bounce from
// the floor, sharp on polished surfaces and spread out on rough ones, with
// Schlick's approximation of Fresnel); and its lights glow, each group as
// bright as its level.
VARYING vec2 mapUv;
VARYING float upperSurface;
VARYING float edgeRound;
VARYING vec3 worldPoint;
VARYING vec3 axisX;
VARYING vec3 axisY;
VARYING vec3 axisZ;
VARYING float facingUp;

// sRGB colours made linear, as built-in materials do.
vec3 linearColour(vec3 c)
{
    return c * (c * (c * 0.305306011 + vec3(0.682171111)) + vec3(0.012522878));
}

float heightAt(vec2 uv)
{
    return texture(shapeMap, uv).r;
}

// A group's light at a point of the surface maps, as blurred as `detail`
// says (a level of its map); its own map covers only its rect of them.
vec3 lightIn(sampler2D map, vec4 rect, vec2 uv, float detail)
{
    vec2 local = (uv - rect.xy) / max(rect.zw, vec2(1e-6));
    if (rect.z <= 0.0 || local.x < 0.0 || local.y < 0.0 || local.x > 1.0 || local.y > 1.0)
        return vec3(0.0);
    return texture(map, local, detail).rgb;
}

float softStep(float v, float lo, float hi, float width)
{
    return clamp((v - lo) / width, 0.0, 1.0) * clamp((hi - v) / width, 0.0, 1.0);
}

// The studio seen along `r`, a reflected ray in the camera's frame (x right,
// y up, z toward the viewer).
float studio(vec3 r)
{
    float softbox = softStep(r.y, 0.58, 1.05, 0.08) * softStep(r.x, -0.5, 0.5, 0.14);
    float strip = softStep(-r.x, 0.72, 1.1, 0.05) * softStep(r.y, -0.3, 0.55, 0.1);
    return 0.02 + 3.2 * softbox + 1.4 * strip + 0.16 * clamp(-r.y - 0.2, 0.0, 1.0);
}

void MAIN()
{
    vec2 uv = mapUv;
    // A wall wears what the decoration is made of at its edge, evened out (a
    // blurrier level of the maps): the edge's small details (a row of studs)
    // would otherwise run down it as streaks. So does a steep side, over
    // which the maps, laid on it from above, stretch. Round a rounded edge
    // (a soft decoration's) the maps are laid round the curve evenly, and
    // only a little evened out, its lights too; a wall's lights are read as
    // they are, or a bright light beside it would spread over it.
    float curve = 1.0 - smoothstep(0.75, 1.0, edgeRound);
    float steep = 1.0 - smoothstep(0.25, 0.7, facingUp);
    float evenOut = mix(3.0, 1.0, curve);
    float detail = upperSurface > 0.5 ? evenOut * steep : evenOut;
    vec4 albedo = texture(albedoMap, uv, detail);
    vec4 finish = texture(finishMap, uv, detail);
    vec4 shape = texture(shapeMap, uv, detail);
    vec3 n = normalize(NORMAL);
    // A wall, seen edge on, shows what the decoration is made of mixed with
    // the empty dark round it, as much of each as covers the round about it
    // (a thin part's walls darker than a broad one's); both evened out along
    // the edge, over a few dozen pixels, as the grain of a material laid
    // along the band (a brushed metal's) and a finely scalloped outline
    // change along the edge, and each change would otherwise run down the
    // wall as a streak. The maps' levels are chosen from the slopes outside
    // the branch, where neighbouring pixels all go the same way.
    float wall = (1.0 - upperSurface) * (1.0 - smoothstep(0.3, 0.6, abs(facingUp))) * (1.0 - curve);
    // So does a steep top by the edge, seen as much from the side.
    float edgeOn = max(wall, upperSurface * steep * (1.0 - curve));
    float covered = texture(shapeMap, uv).b;
    vec2 baseX = dFdx(uv), baseY = dFdy(uv);
    vec2 slopeX = baseX * exp2(detail), slopeY = baseY * exp2(detail);
    if (wall > 0.01) {
        vec2 outward = vec2(dot(n, axisX), -dot(n, axisY));
        vec2 along = normalize(vec2(-outward.y, outward.x) + vec2(1e-6, 0.0)) * mapTexel * 3.0;
        vec4 albedoAlong = vec4(0.0), finishAlong = vec4(0.0);
        float coveredAlong = 0.0;
        for (int k = -6; k <= 6; ++k) {
            vec2 at = uv + along * float(k);
            albedoAlong += textureGrad(albedoMap, at, slopeX, slopeY);
            finishAlong += textureGrad(finishMap, at, slopeX, slopeY);
            // (How much is covered read sharp: a blurrier level's squares
            // would bring the outline's steps back.)
            coveredAlong += textureGrad(shapeMap, at, baseX, baseY).b;
        }
        albedo = mix(albedo, albedoAlong / 13.0, wall);
        finish = mix(finish, finishAlong / 13.0, wall);
        covered = mix(covered, coveredAlong / 13.0, wall);
    }
    float held = mix(1.0, clamp(covered, 0.0, 1.0), edgeOn);
    float lit = 1.0;
    if (upperSurface > 0.5) {
        // The relief's slope over a pixel either way: down the maps is down
        // the solid, so their v turns over.
        float hx = (heightAt(uv + vec2(mapTexel.x, 0.0)) - heightAt(uv - vec2(mapTexel.x, 0.0))) * 0.5;
        float hy = (heightAt(uv + vec2(0.0, mapTexel.y)) - heightAt(uv - vec2(0.0, mapTexel.y))) * 0.5;
        vec3 fine = normalize(vec3(-hx, hy, 1.0));
        // On a steep side the grain gives way to the solid's own roundness,
        // and so it does round a rounded edge's curve, whose shape the solid
        // itself carries.
        n = normalize(mix(axisX * fine.x + axisY * fine.y + axisZ * fine.z, n, max(steep, curve)));
        NORMAL = n;
        // Shadowed where the relief rises between here and the key light,
        // traced along the maps toward it.
        vec3 toward = normalize(lightInSolid);
        float planar = length(toward.xy);
        if (planar > 1e-3 && toward.z > 0.02) {
            vec2 way = vec2(toward.x, -toward.y) / planar;
            float rise = toward.z / planar;
            float stride = max(1.0, halfWidthPixels / 16.0);
            float soft = max(1.0, halfWidthPixels * 0.06);
            float here = shape.r;
            float blocked = 0.0;
            for (int k = 1; k <= 24; ++k) {
                float d = float(k) * stride;
                blocked = max(blocked, (heightAt(uv + way * d * mapTexel) - here - d * rise) / soft);
            }
            lit = 1.0 - clamp(blocked, 0.0, 1.0) * 0.75;
        }
    }
    vec3 colour = linearColour(albedo.rgb) * held;
    float occlusion = albedo.a * held;
    float metal = finish.r * held;
    float rough = mix(0.5, finish.g, held);
    float coat = finish.b * held;
    float tent = max(0.0, finish.a * 3.0 - 1.0) * held;
    // A light's own face shows its light, not the room's; what gives its own
    // light (a nebula's picture) gives that share of its colour itself, the
    // scene's light the rest.
    float own = shape.a;
    float face = (1.0 - 0.85 * shape.g) * (1.0 - 0.85 * own);
    BASE_COLOR = vec4(colour * face * lit, 1.0);
    METALNESS = metal;
    ROUGHNESS = max(rough, 0.04);
    CLEARCOAT_AMOUNT = coat;
    CLEARCOAT_ROUGHNESS = 0.06;
    OCCLUSION_AMOUNT = occlusion;
    SPECULAR_AMOUNT = 0.5;

    // The studio mirrored: it stands round the viewer, so the reflected ray
    // is read in the camera's own frame (the camera orbits the panel).
    vec3 view = normalize(CAMERA_POSITION - worldPoint);
    vec3 reflected = reflect(-view, n);
    vec3 r = normalize((viewMatrix * vec4(reflected, 0.0)).xyz);
    float sharp = studio(r);
    float blur = clamp(rough * 1.7, 0.0, 1.0);
    float room = sharp * (1.0 - blur) + 0.7 * blur + tent * 0.85;
    float facing = clamp(dot(n, view), 0.0, 1.0);
    float grazing = 1.0 - facing;
    float fresnel = grazing * grazing * grazing * grazing * grazing;
    vec3 base = mix(vec3(0.04), colour, metal);
    vec3 mirrored = room * (base + (vec3(1.0) - base) * fresnel) * (0.35 + 0.65 * metal) * occlusion * face;
    float coatShare = (0.05 + 0.95 * fresnel) * coat;
    mirrored = mirrored * (1.0 - coatShare) + vec3(sharp * coatShare * 1.6 * occlusion);

    float lightDetail = detail * curve;
    vec3 lights = lightIn(lightsMap, lightsRect, uv, lightDetail) * lightsLevel
        + lightIn(starsMap, starsRect, uv, lightDetail) * starsLevel
        + lightIn(starsBMap, starsBRect, uv, lightDetail) * starsBLevel
        + lightIn(blinkAMap, blinkARect, uv, lightDetail) * blinkALevel
        + lightIn(blinkBMap, blinkBRect, uv, lightDetail) * blinkBLevel
        + lightIn(arcsMap, arcsRect, uv, lightDetail) * arcsLevel;
    // The round light under the pointer lights up: through from a white core
    // in its own colour, brighter than full light, so the scene's bloom
    // spreads its halo round it.
    vec2 fromSpot = (uv - hoverSpot.xy) / max(hoverSpot.zw * 1.15, vec2(1e-6));
    float spot = 1.0 - smoothstep(0.0, 1.0, length(fromSpot));
    vec3 litUp = mix(hoverColour, vec3(1.0), 0.25 * (1.0 - smoothstep(0.0, 0.35, length(fromSpot))));
    vec3 hover = litUp * spot * spot * hoverLevel * 3.5 * step(1e-6, hoverSpot.z);
    EMISSIVE_COLOR = mirrored + colour * own + lights * emissionScale + hover;
}
