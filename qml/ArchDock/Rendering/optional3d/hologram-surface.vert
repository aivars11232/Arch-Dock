// A hologram's surface: where each point is in the scene, its normal there
// and its picture's coordinate.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;

void MAIN()
{
    pictureUv = UV0;
    worldPoint = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    worldNormal = normalize(NORMAL_MATRIX * NORMAL);
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
