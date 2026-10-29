// A 3D icon made into an object (icon-surface.frag): the colour each point
// carries.
VARYING vec3 pointColour;

void MAIN()
{
    pointColour = COLOR.rgb;
    POSITION = MODELVIEWPROJECTION_MATRIX * vec4(VERTEX, 1.0);
}
