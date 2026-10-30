#version 440
// A line drawing (a status icon) given a body: every pixel takes the
// strongest of its neighbours within `reach` (in the picture's own size,
// across and down), so its strokes are drawn thicker and keep their colour.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 reach;
};
layout(binding = 1) uniform sampler2D source;

void main()
{
    vec4 strongest = texture(source, qt_TexCoord0);
    // Three rings of sixteen samples round the pixel.
    for (int ring = 1; ring <= 3; ++ring) {
        float share = float(ring) / 3.0;
        for (int step = 0; step < 16; ++step) {
            float angle = 6.2831853 * (float(step) + 0.5 * float(ring)) / 16.0;
            vec4 near = texture(source, qt_TexCoord0 + vec2(cos(angle), sin(angle)) * reach * share);
            if (near.a > strongest.a)
                strongest = near;
        }
    }
    fragColor = strongest * qt_Opacity;
}
