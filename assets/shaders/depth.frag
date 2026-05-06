#version 430 core
in vec3 vViewPos;
uniform mat4 projectionMatrix;
uniform float pointRadius;

void main() {
    // gl_PointCoord [0,1]をローカル座標 [-1,1]にマップ
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard; // 球面ディスク外のフラグメントを破棄

    // 球の表面法線を再構築
    vec3 normal = vec3(coord.x, coord.y, sqrt(1.0 - r2));

    // このピクセルの3次元ビュー空間位置を再構築
    vec4 pixelViewPos = vec4(vViewPos + normal * pointRadius, 1.0);
    vec4 clipSpacePos = projectionMatrix * pixelViewPos;

    // NDC深度をウィンドウ空間 [0,1]に変換して深度バッファに書き込む
    float ndcDepth = clipSpacePos.z / clipSpacePos.w;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;
}
