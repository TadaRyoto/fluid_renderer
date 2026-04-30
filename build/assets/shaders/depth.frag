#version 430 core
in vec3 vViewPos;
uniform mat4 projectionMatrix;
uniform float pointRadius;

void main() {
    // Map gl_PointCoord [0,1] to local coords [-1,1].
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard; // Discard fragments outside the sphere disc.

    // Reconstruct surface normal of the sphere.
    vec3 normal = vec3(coord.x, coord.y, sqrt(1.0 - r2));

    // Reconstruct the 3-D view-space position of this pixel.
    vec4 pixelViewPos = vec4(vViewPos + normal * pointRadius, 1.0);
    vec4 clipSpacePos = projectionMatrix * pixelViewPos;

    // Convert NDC depth to window-space [0,1] and write to depth buffer.
    float ndcDepth = clipSpacePos.z / clipSpacePos.w;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;
}
