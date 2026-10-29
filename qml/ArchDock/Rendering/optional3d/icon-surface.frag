// A 3D icon made into an object: each point in the colour the object has
// there, taken as it comes (as a built-in material takes a mesh's own
// colours), modelled by the lamps, with as much of it besides as Icon
// lighting keeps for its own.
VARYING vec3 pointColour;

void MAIN()
{
    BASE_COLOR = vec4(pointColour, 1.0);
    METALNESS = 0.0;
    ROUGHNESS = roughness;
    EMISSIVE_COLOR = pointColour * own;
}
