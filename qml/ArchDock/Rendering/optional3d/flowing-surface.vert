// The band's texture coordinates along and across it, and its tangent
// frame in world space, for the ripples to bend the light in.
VARYING vec2 flowUv;
VARYING vec3 flowTangent;
VARYING vec3 flowBinormal;

void MAIN()
{
    flowUv = vec2(UV0.x * repeatU, UV0.y);
    flowTangent = normalize((MODEL_MATRIX * vec4(TANGENT, 0.0)).xyz);
    flowBinormal = normalize((MODEL_MATRIX * vec4(BINORMAL, 0.0)).xyz);
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
