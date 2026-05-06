#version 430 core
layout(location = 0) in vec3 inPosition;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform float pointRadius;
uniform float screenHeight;

out vec3 vViewPos;

void main() {
    // 頂点をビュー空間に変換
    vec4 viewPos = viewMatrix * vec4(inPosition, 1.0);
    vViewPos = viewPos.xyz;

    // カメラからの距離
    float dist = length(viewPos.xyz);

    // ポイントサイズ(ピクセル単位): 球の半径をスクリーンに投影
    gl_PointSize = pointRadius * (screenHeight / dist) * projectionMatrix[1][1];
    gl_Position = projectionMatrix * viewPos;
}
