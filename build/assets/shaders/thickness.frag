#version 430 core
out float FragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard;

    // Sphere half-thickness at this texel (1.0 at centre, 0.0 at edge).
    // Accumulated additively across all particles to build a thickness map.
    float thickness = sqrt(1.0 - r2);
    FragColor = thickness * 0.1;
}
