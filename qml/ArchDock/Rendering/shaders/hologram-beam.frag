#version 440
// A projector's light: rising from its opening and widening as it rises
// (from `startHalf` to `endHalf`, each half its width as a share of the
// sheet's), soft at its sides and fading as it rises (as steeply as `fade`
// says), made of fine streaks and a few sparks. Its motion (`motion`):
// x the streaks' light rising in waves and the sparks rising, y its rays
// drifting sideways in two layers at their own rates (the Godot "God Rays"
// shader's method, CC0), z rings of light rising through it, w a bright
// band sweeping from side to side; none of them for a still light. Its
// colour is `tint`, or where `own` says the picture's own main colour.
// The softness and fading follow the Godot "Spatial Light Shaft" shader
// (CC0).
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 tint;
    vec4 motion;
    float time;
    float startHalf;
    float endHalf;
    float streaks;
    float sparks;
    float seed;
    float fade;
    float strength;
    float own;
};
layout(binding = 1) uniform sampler2D source;

float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233)) + seed) * 43758.5453); }

// The picture's main colour: its covered parts' average, as bright as light.
vec3 mainColour()
{
    vec4 sum = vec4(0.0);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            sum += texture(source, vec2(0.2 + 0.2 * float(i), 0.2 + 0.2 * float(j)));
    vec3 average = sum.a > 1e-3 ? sum.rgb / sum.a : vec3(1.0);
    return average / max(max(average.r, average.g), max(average.b, 0.2));
}

// One layer of streaks: thin bright columns, some of them lit.
float streakLayer(float x, float count, float shift, float up, float phase)
{
    float position = x * count + shift;
    float column = floor(position);
    float h = hash(vec2(column, 3.0 + phase));
    float waves = mix(0.8, 0.55 + 0.45 * sin(up * 9.0 - time * (1.5 + 2.5 * h) + h * 40.0), motion.x);
    float thin = 1.0 - abs(fract(position) - 0.5) * 2.0;
    return step(0.55, h) * waves * pow(thin, 3.0);
}

void main()
{
    float up = 1.0 - qt_TexCoord0.y;
    float half_ = mix(startHalf, endHalf, pow(up, 0.85));
    float across = (qt_TexCoord0.x - 0.5) / half_;
    float body = 1.0 - smoothstep(0.55, 1.0, abs(across));
    float fading = pow(1.0 - up, fade);
    float x = across * 0.5 + 0.5;
    // The rays drift sideways and back, the fine ones faster.
    float driftA = motion.y * sin(time * 0.35 + seed) * 1.6;
    float driftB = motion.y * sin(time * 0.6 + seed * 1.7 + 1.0) * 2.4;
    float streak = streakLayer(x, streaks, driftA, up, 0.0)
        + 0.35 * streakLayer(x, streaks * 2.6, driftB, up, 5.0);
    float core = 1.0 - smoothstep(0.0, 0.8, abs(across));
    float lit = body * fading * (0.55 + 1.4 * streak + 0.45 * core);
    // Rings of light rising through it.
    float ring = smoothstep(0.86, 1.0, 0.5 + 0.5 * sin((up * 3.0 - time * 0.9) * 6.2831853));
    lit *= 1.0 + motion.z * 1.3 * ring;
    // A bright band sweeping across it.
    float band = exp(-pow((across - 0.85 * sin(time * 0.8 + seed)) / 0.28, 2.0));
    lit *= 1.0 + motion.w * 1.4 * band;
    vec2 grid = vec2(x * 9.0, up * 14.0 - time * 1.2 * motion.x);
    vec2 cell = floor(grid);
    float r = hash(cell);
    float spark = step(1.0 - sparks, r)
        * smoothstep(0.16, 0.0, length(fract(grid) - vec2(0.5 + 0.3 * (hash(cell + 7.0) - 0.5), 0.5)));
    float light = (lit + spark * body * fading * 1.4) * strength;
    vec3 colour = mix(tint.rgb, mainColour(), own);
    fragColor = vec4(colour * light * 1.2, light * 0.8) * qt_Opacity;
}
