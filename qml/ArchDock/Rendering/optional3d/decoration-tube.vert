// A decoration's tubes: their points folded with the platform. The fragment
// shader gets the bark's texture coordinate, whether the point is on the
// bark, a cut end or a mark on the bark (whose colour comes with it), and
// where it is in the scene.
VARYING vec2 barkUv;
VARYING float kind;
VARYING float markBlue;
VARYING vec3 worldPoint;

void MAIN()
{
    vec3 p = VERTEX;
    vec3 n = NORMAL;
    // The far half (y above the middle) turned about the hinge at the
    // ground, as PlatformGeometry.foldPoint turns the platform.
    if (abs(foldTurn) > 1e-6 && p.y > 0.0) {
        float c = cos(foldTurn);
        float s = sin(foldTurn);
        p = vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
        n = vec3(n.x, n.y * c - n.z * s, n.y * s + n.z * c);
    }
    barkUv = UV0;
    kind = UV1.x;
    markBlue = UV1.y;
    worldPoint = (MODEL_MATRIX * vec4(p, 1.0)).xyz;
    NORMAL = n;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(p, 1.0);
}
