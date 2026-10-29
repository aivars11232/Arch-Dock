// An object's hologram (hologram-object.frag): each point's own colour,
// where it is in the scene and in the object's own frame, and its normal.
VARYING vec3 pointColour;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;
VARYING vec3 shapePoint;

void MAIN()
{
    pointColour = COLOR.rgb;
    worldPoint = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
    worldNormal = normalize(NORMAL_MATRIX * NORMAL);
    // The object's own frame, its depth measured as its width is (as
    // hologram-solid.vert): the model stretches its depth alone.
    float across = length(MODEL_MATRIX[0].xyz);
    float deep = length(MODEL_MATRIX[2].xyz);
    shapePoint = vec3(VERTEX.xy, VERTEX.z * deep / max(across, 1e-6));
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
