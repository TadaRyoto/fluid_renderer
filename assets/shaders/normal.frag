#version 430 core
// 平滑化された深度テクスチャからビュー空間法線を再構築
// ビュー空間位置の有限差分を使用して、各軸で|dz|が小さい隣接ピクセルを選択し、
// シルエットエッジのスパイクを避ける
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D depthTex;          // 平滑化された深度、R32F (ウィンドウ空間 [0,1])
uniform vec2 texelSize;              // (1/width, 1/height)
uniform mat4 invProjectionMatrix;    // 透視投影の逆行列

// (uv, ウィンドウ空間深度) -> ビュー空間位置に変換
vec3 uvToView(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjectionMatrix * clip;
    return view.xyz / view.w;
}

void main() {
    float dC = texture(depthTex, TexCoord).r;

    // 背景 -> 法線をゼロにマークしてコンポジット段階が検出できるようにする
    if (dC >= 1.0) {
        FragColor = vec4(0.0);
        return;
    }

    vec3 posC = uvToView(TexCoord, dC);

    // 隣接ピクセルをサンプリング (左/右/上/下)
    float dL = texture(depthTex, TexCoord - vec2(texelSize.x, 0.0)).r;
    float dR = texture(depthTex, TexCoord + vec2(texelSize.x, 0.0)).r;
    float dB = texture(depthTex, TexCoord - vec2(0.0, texelSize.y)).r;
    float dT = texture(depthTex, TexCoord + vec2(0.0, texelSize.y)).r;

    // 前進/後進差分。背景に接する隣接ピクセルを"無限遠"として扱って
    // |dz|比較で常に負けるようにする
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

    // 法線がカメラに向いていることを確認 (ビュー空間カメラは-Z方向を見る)
    if (n.z < 0.0) n = -n;

    FragColor = vec4(n, 1.0);
}
