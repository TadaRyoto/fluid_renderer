#version 430 core
out float FragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard;
    
    // 半球の厚みを計算し、加算合成するために出力
    float thickness = sqrt(1.0 - r2);
    FragColor = thickness * 1e-12; // 白飛びを防ぐためのスケール係数
}