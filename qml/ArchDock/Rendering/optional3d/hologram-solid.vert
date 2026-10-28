// A 3D icon's hologram (hologram-solid.frag): where each point is in the
// scene and in the solid's own frame, its normal there and its picture's
// coordinate.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;
VARYING vec3 shapePoint;

void MAIN()
{
    pictureUv = UV0;
    worldPoint = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    worldNormal = normalize(NORMAL_MATRIX * NORMAL);
    // The solid's own frame (its picture 100 across, up its picture's up,
    // toward the eye out of its front), its depth measured as its width is:
    // the model stretches its depth alone (Icon depth).
    float across = length(MODEL_MATRIX[0].xyz);
    float deep = length(MODEL_MATRIX[2].xyz);
    shapePoint = vec3(VERTEX.xy, VERTEX.z * deep / max(across, 1e-6));
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
