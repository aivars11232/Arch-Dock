// A hologram's glow round its solid (HologramHalo): the inside of a shell a
// little larger than the solid, its light strongest where the shell faces
// the eye, straight behind the solid, and fading to nothing where it turns
// away, at its edge.
VARYING vec2 pictureUv;
VARYING vec3 worldPoint;
VARYING vec3 worldNormal;

void MAIN()
{
    vec3 toEye = normalize(CAMERA_POSITION - worldPoint);
    float facing = abs(dot(normalize(worldNormal), toEye));
    FRAGCOLOR = vec4(glowColour.rgb * pow(facing, 2.5) * strength, 0.0);
}
