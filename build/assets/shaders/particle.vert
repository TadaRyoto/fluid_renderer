#version 430 core
layout(location = 0) in vec3 inPosition;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform float pointRadius;
uniform float screenHeight;

out vec3 vViewPos;

void main() {
    // Transform vertex to view space.
    vec4 viewPos = viewMatrix * vec4(inPosition, 1.0);
    vViewPos = viewPos.xyz;

    // Distance from camera.
    float dist = length(viewPos.xyz);

    // Point size in pixels: project sphere radius onto screen.
    gl_PointSize = pointRadius * (screenHeight / dist) * projectionMatrix[1][1];
    gl_Position = projectionMatrix * viewPos;
}
