// A panel decoration's solid (ArchDock.Decorations3D's DecorationSolid): its
// points folded with the platform. The fragment shader gets the surface
// maps' coordinates, whether the point is on the upper surface, how far round
// the curve along the outline it lies, and the solid's own axes in the
// scene, to turn the maps' slopes into the scene's normals.
VARYING vec2 mapUv;
VARYING float upperSurface;
VARYING float edgeRound;
VARYING vec3 worldPoint;
VARYING vec3 axisX;
VARYING vec3 axisY;
VARYING vec3 axisZ;
VARYING float facingUp;

void MAIN()
{
    vec3 p = VERTEX;
    vec3 n = NORMAL;
    vec3 ax = vec3(1.0, 0.0, 0.0);
    vec3 ay = vec3(0.0, 1.0, 0.0);
    vec3 az = vec3(0.0, 0.0, 1.0);
    // The far half (y above the middle) turned about the hinge at the
    // ground, as PlatformGeometry.foldPoint turns the platform.
    if (abs(foldTurn) > 1e-6 && p.y > 0.0) {
        float c = cos(foldTurn);
        float s = sin(foldTurn);
        p = vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
        n = vec3(n.x, n.y * c - n.z * s, n.y * s + n.z * c);
        ay = vec3(0.0, c, s);
        az = vec3(0.0, -s, c);
    }
    mapUv = UV0;
    upperSurface = UV1.x;
    edgeRound = UV1.y;
    // How much the solid faces up here (a round solid's sides do not).
    facingUp = NORMAL.z;
    worldPoint = (MODEL_MATRIX * vec4(p, 1.0)).xyz;
    axisX = normalize(NORMAL_MATRIX * ax);
    axisY = normalize(NORMAL_MATRIX * ay);
    axisZ = normalize(NORMAL_MATRIX * az);
    NORMAL = n;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(p, 1.0);
}
