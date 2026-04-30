#version 430 core
out float FragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard;

    // Sphere half-thickness, accumulated additively across overlapping point
    // sprites to build the screen-space thickness map. Output is intentionally
    // tiny; the composite stage multiplies by `thicknessScale` (~1e10) to
    // bring the value back into a useful range while keeping the R32F target
    // from saturating under heavy overlap.
    float thickness = sqrt(1.0 - r2);
    FragColor = thickness * 1e-12;
}
