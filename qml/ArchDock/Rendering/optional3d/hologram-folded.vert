// A hologram's surface folded with the platform, as a panel decoration's
// solid and tubes are (decoration-surface.vert): the far half (y above the
// middle) turned about the hinge at the ground. The fragment shader gets
// where each point is in the scene, its normal there and its picture's
// coordinate, as hologram-surface.vert gives them.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;

void MAIN()
{
    vec3 p = VERTEX;
    vec3 n = NORMAL;
    if (abs(foldTurn) > 1e-6 && p.y > 0.0) {
        float c = cos(foldTurn);
        float s = sin(foldTurn);
        p = vec3(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
        n = vec3(n.x, n.y * c - n.z * s, n.y * s + n.z * c);
    }
    pictureUv = UV0;
    worldPoint = (MODEL_MATRIX * vec4(p, 1.0)).xyz;
    worldNormal = normalize(NORMAL_MATRIX * n);
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(p, 1.0);
}
