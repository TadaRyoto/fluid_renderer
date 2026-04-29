#version 430 core
in vec3 vViewPos;
uniform mat4 projectionMatrix;
uniform float pointRadius;

void main() {
    // gl_PointCoord (0.0~1.0) をローカル座標系 (-1.0~1.0) に変換
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard; // 円の外側を破棄して丸く切り抜く
    
    // 球面のZ成分と法線を計算
    vec3 normal = vec3(coord.x, coord.y, sqrt(1.0 - r2));
    
    // ピクセルの視点空間上の3D座標を再構築
    vec4 pixelViewPos = vec4(vViewPos + normal * pointRadius, 1.0);
    vec4 clipSpacePos = projectionMatrix * pixelViewPos;
    
    // 正規化デバイス座標からウィンドウ座標の深度値(0.0~1.0)へ変換して書き込み
    float ndcDepth = clipSpacePos.z / clipSpacePos.w;
    gl_FragDepth = ndcDepth * 0.5 + 0.5;
}