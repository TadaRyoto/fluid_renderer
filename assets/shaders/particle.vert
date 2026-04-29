#version 430 core
layout(location = 0) in vec3 inPosition;

uniform mat4 viewMatrix;
uniform mat4 projectionMatrix;
uniform float pointRadius;
uniform float screenHeight;

out vec3 vViewPos;

void main() {
    // 頂点を視点空間（View Space）に変換
    vec4 viewPos = viewMatrix * vec4(inPosition, 1.0);
    vViewPos = viewPos.xyz;
    
    // カメラからの距離
    float dist = length(viewPos.xyz);
    
    // スクリーンの高さとカメラ距離からポイントスプライトのサイズを計算
    gl_PointSize = pointRadius * (screenHeight / dist) * projectionMatrix[1][1];
    gl_Position = projectionMatrix * viewPos;
}