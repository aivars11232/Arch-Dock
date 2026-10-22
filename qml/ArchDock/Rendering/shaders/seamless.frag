#version 440
// A picture made to wrap round a closed band without a seam: near its two
// ends it fades into a copy of itself shifted by half its width, so both
// ends show the same strip of the picture and meet where the band closes.
// This is the usual "offset and blend" way of making a photograph tile; the
// shifted copy's own join lies in the picture's middle, where it is not used.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    // How far from each end the fade reaches, as a share of the width.
    float blend;
};
layout(binding = 1) uniform sampler2D source;

void main()
{
    vec2 uv = qt_TexCoord0;
    vec4 own = texture(source, uv);
    vec4 shifted = texture(source, vec2(fract(uv.x + 0.5), uv.y));
    float weight = 1.0 - smoothstep(0.0, blend, min(uv.x, 1.0 - uv.x));
    fragColor = mix(own, shifted, weight) * qt_Opacity;
}
