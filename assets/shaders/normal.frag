#version 430 core
// Reconstructs view-space normals from the smoothed depth texture.
// Uses finite differences on view-space positions, choosing the smaller
// |dz| neighbor on each axis to avoid spikes at silhouette edges.
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D depthTex;          // smoothed depth, R32F (window-space [0,1])
uniform vec2 texelSize;              // (1/width, 1/height)
uniform mat4 invProjectionMatrix;    // inverse of the perspective projection

// Convert (uv, window-space depth) -> view-space position.
vec3 uvToView(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjectionMatrix * clip;
    return view.xyz / view.w;
}

void main() {
    float dC = texture(depthTex, TexCoord).r;

    // Background -> mark normal as zero so the composite stage can detect it.
    if (dC >= 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 posC = uvToView(TexCoord, dC);

    // Sample neighbors (left/right/top/bottom)
    float dL = texture(depthTex, TexCoord - vec2(texelSize.x, 0.0)).r;
    float dR = texture(depthTex, TexCoord + vec2(texelSize.x, 0.0)).r;
    float dB = texture(depthTex, TexCoord - vec2(0.0, texelSize.y)).r;
    float dT = texture(depthTex, TexCoord + vec2(0.0, texelSize.y)).r;

    // Forward / backward differences. Treat background-touching neighbors as
    // "infinitely far" so they always lose the |dz| comparison.
    vec3 ddxR = (dR < 1.0) ? (uvToView(TexCoord + vec2(texelSize.x, 0.0), dR) - posC)
                           : vec3(1e10);
    vec3 ddxL = (dL < 1.0) ? (posC - uvToView(TexCoord - vec2(texelSize.x, 0.0), dL))
                           : vec3(1e10);
    vec3 ddyT = (dT < 1.0) ? (uvToView(TexCoord + vec2(0.0, texelSize.y), dT) - posC)
                           : vec3(1e10);
    vec3 ddyB = (dB < 1.0) ? (posC - uvToView(TexCoord - vec2(0.0, texelSize.y), dB))
                           : vec3(1e10);

    vec3 ddx = (abs(ddxR.z) < abs(ddxL.z)) ? ddxR : ddxL;
    vec3 ddy = (abs(ddyT.z) < abs(ddyB.z)) ? ddyT : ddyB;

    vec3 n = normalize(cross(ddx, ddy));

    // Make sure the normal faces the camera (view-space camera looks down -Z).
    if (n.z < 0.0) n = -n;

    FragColor = vec4(n, 1.0);
}
