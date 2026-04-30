#version 430 core
// Final fluid shading. Combines the smoothed depth, reconstructed normal and
// thickness with a procedural environment to produce refraction, reflection
// (Fresnel-weighted), Beer-Lambert color absorption and a specular highlight.
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D depthTex;        // smoothed depth (R32F, window-space [0,1])
uniform sampler2D normalTex;       // view-space normal (RGB16F)
uniform sampler2D thicknessTex;    // accumulated thickness (R32F)
uniform mat4 invProjectionMatrix;  // inverse perspective
uniform float thicknessScale;      // brings raw thickness into a useful range

// --- Tunable shading constants ----------------------------------------------
const vec3  LIGHT_DIR        = normalize(vec3(0.4, 0.7, 0.6)); // view space
const vec3  FLUID_TINT       = vec3(0.02, 0.25, 0.65);
const vec3  ABSORPTION_COEFF = vec3(4.0, 1.2, 0.15);           // R/G/B per unit thickness
const float IOR              = 1.33;                           // water
const float F0               = 0.02;                           // Schlick base reflectance
const float SHININESS        = 128.0;
const float REFRACT_OFFSET   = 0.05; // screen-space refraction distortion strength

// Procedural "environment" sampled by direction. Stands in for a real cubemap
// / scene texture so refraction and reflection have something to fetch.
vec3 envSample(vec3 dir) {
    dir = normalize(dir);

    // Sky gradient
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(vec3(0.70, 0.82, 0.95), vec3(0.15, 0.30, 0.60), t);

    // Sun disc
    float sun = pow(max(dot(dir, LIGHT_DIR), 0.0), 256.0);
    sky += vec3(1.0, 0.9, 0.7) * sun;

    // Checkerboard floor below y = 0
    if (dir.y < -0.02) {
        float scale = 2.0 / (-dir.y);
        vec2 p = dir.xz * scale;
        float c = mod(floor(p.x) + floor(p.y), 2.0);
        vec3 floorCol = mix(vec3(0.55, 0.50, 0.45), vec3(0.20, 0.18, 0.16), c);

        // Soft horizon blend so the seam is not visible.
        float blend = smoothstep(-0.15, -0.02, dir.y);
        return mix(floorCol, sky, blend);
    }
    return sky;
}

// (uv, NDC-z=0) -> normalized view-space ray direction. Used for background
// pixels where we have no depth.
vec3 viewDirFromUV(vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 v = invProjectionMatrix * clip;
    return normalize(v.xyz / v.w);
}

void main() {
    float depth = texture(depthTex, TexCoord).r;
    vec3  bgDir = viewDirFromUV(TexCoord);

    // No fluid here -> just show the background.
    if (depth >= 1.0) {
        FragColor = vec4(envSample(bgDir), 1.0);
        return;
    }

    vec3 N = texture(normalTex, TexCoord).xyz;
    if (dot(N, N) < 1e-6) {
        FragColor = vec4(envSample(bgDir), 1.0);
        return;
    }
    N = normalize(N);

    // View-space surface position from depth.
    vec4 clip = vec4(TexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjectionMatrix * clip;
    vec3 P = view.xyz / view.w;
    vec3 V = normalize(-P);                       // surface -> camera

    // Thickness (additive blend output) brought into a sensible range.
    float thickness = max(texture(thicknessTex, TexCoord).r * thicknessScale, 0.0);

    // ------------- Refraction -----------------------------------------------
    // Cheap screen-space distortion: offset UV by the surface normal's xy,
    // weighted by thickness so deeper fluid bends light more.
    vec2 refractUV = TexCoord + N.xy * REFRACT_OFFSET * thickness;
    vec3 refractDir = refract(-V, N, 1.0 / IOR);
    if (dot(refractDir, refractDir) < 1e-6) refractDir = -V; // total internal reflection fallback
    vec3 refractedEnv = envSample(refractDir);

    // Use the offset UV to sample the env via the direction at that UV — this
    // keeps the method texture-driven (would naturally extend to a real
    // background scene texture).
    vec3 refractedScene = envSample(viewDirFromUV(refractUV));
    vec3 refracted = mix(refractedEnv, refractedScene, 0.5);

    // ------------- Beer-Lambert absorption ----------------------------------
    // Light traveling through the fluid loses wavelengths according to
    // exp(-coeff * thickness). The fluid's own tint is added in proportion to
    // the absorbed light (single-scattering approximation).
    vec3 attenuation = exp(-ABSORPTION_COEFF * thickness);
    vec3 transmission = refracted * attenuation + FLUID_TINT * (1.0 - attenuation);

    // ------------- Reflection + Fresnel ------------------------------------
    vec3 reflectDir = reflect(-V, N);
    vec3 reflected = envSample(reflectDir);

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // ------------- Specular highlight (Blinn-Phong) -------------------------
    vec3 H = normalize(LIGHT_DIR + V);
    float spec = pow(max(dot(N, H), 0.0), SHININESS);

    vec3 color = mix(transmission, reflected, fresnel) + spec * vec3(1.2);

    FragColor = vec4(color, 1.0);
}
