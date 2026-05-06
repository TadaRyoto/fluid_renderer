#version 430 core
// 最終流体シェーディング。平滑化された深度、再構築された法線と厚さを
// 手続き的な環境と組み合わせて、屈折、反射(Fresnel加重)、
// Beer-Lambert色吸収、鏡面ハイライトを生成
out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D depthTex;        // 平滑化された深度 (R32F、ウィンドウ空間 [0,1])
uniform sampler2D normalTex;       // ビュー空間法線 (RGB16F)
uniform sampler2D thicknessTex;    // 累積厚さ (R32F)
uniform mat4 invProjectionMatrix;  // 透視投影の逆行列
uniform float thicknessScale;      // 生の厚さを有用な範囲に変換

// --- 調整可能なシェーディング定数 ------------------------------------------
const vec3  LIGHT_DIR        = normalize(vec3(0.4, 0.7, 0.6)); // ビュー空間
const vec3  FLUID_TINT       = vec3(0.02, 0.25, 0.65);
const vec3  ABSORPTION_COEFF = vec3(4.0, 1.2, 0.15);           // 単位厚さあたりのR/G/B
const float IOR              = 1.33;                           // 水
const float F0               = 0.02;                           // Schlick基本反射率
const float SHININESS        = 128.0;
const float REFRACT_OFFSET   = 0.05; // スクリーン空間屈折歪み強度

// 方向でサンプリングされる手続き的な"環境"。実際のキューブマップ
// /シーンテクスチャの代わりとなって、屈折と反射がサンプリングする対象を提供
vec3 envSample(vec3 dir) {
    dir = normalize(dir);

    // 空のグラデーション
    float t = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 sky = mix(vec3(0.70, 0.82, 0.95), vec3(0.15, 0.30, 0.60), t);

    // 太陽円盤
    float sun = pow(max(dot(dir, LIGHT_DIR), 0.0), 256.0);
    sky += vec3(1.0, 0.9, 0.7) * sun;

    // y = 0以下のチェッカーボード床
    if (dir.y < -0.02) {
        float scale = 2.0 / (-dir.y);
        vec2 p = dir.xz * scale;
        float c = mod(floor(p.x) + floor(p.y), 2.0);
        vec3 floorCol = mix(vec3(0.55, 0.50, 0.45), vec3(0.20, 0.18, 0.16), c);

        // 柔らかい地平線ブレンドでシームが見えないようにする
        float blend = smoothstep(-0.15, -0.02, dir.y);
        return mix(floorCol, sky, blend);
    }
    return sky;
}

// (uv, NDC-z=0) -> 正規化されたビュー空間光線方向。深度がない背景
// ピクセルで使用される
vec3 viewDirFromUV(vec2 uv) {
    vec4 clip = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
    vec4 v = invProjectionMatrix * clip;
    return normalize(v.xyz / v.w);
}

void main() {
    float depth = texture(depthTex, TexCoord).r;
    vec3  bgDir = viewDirFromUV(TexCoord);

    // ここに流体はない -> 背景を表示するだけ
    if (depth >= 1.0) {
        FragColor = vec4(envSample(bgDir), 1.0);
        return;
    }

    vec3 N = texture(normalTex, TexCoord).xyz;
    if (dot(N, N) < 1e-6) {
        FragColor = vec4(envSample(bgDir), 1.0);
        return;
    }
    N = normalize(N);

    // 深度からビュー空間の表面位置を取得
    vec4 clip = vec4(TexCoord * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = invProjectionMatrix * clip;
    vec3 P = view.xyz / view.w;
    vec3 V = normalize(-P);                       // 表面 -> カメラ

    // 厚さ(加算合成出力)を有用な範囲に変換
    float thickness = max(texture(thicknessTex, TexCoord).r * thicknessScale, 0.0);

    // ------------- 屈折 ---------------------------------------------------
    // 安価なスクリーン空間歪み: UVを表面法線のxy方向にオフセット、
    // 厚さで重み付けして、より深い流体がより光を曲げるようにする
    vec2 refractUV = TexCoord + N.xy * REFRACT_OFFSET * thickness;
    vec3 refractDir = refract(-V, N, 1.0 / IOR);
    if (dot(refractDir, refractDir) < 1e-6) refractDir = -V; // 全反射フォールバック
    vec3 refractedEnv = envSample(refractDir);

    // オフセットされたUVを使って、そのUVでの方向経由で環境をサンプリング —
    // これによりメソッドがテクスチャ駆動のままになる(実際の背景
    // シーンテクスチャに自然に拡張可能)
    vec3 refractedScene = envSample(viewDirFromUV(refractUV));
    vec3 refracted = mix(refractedEnv, refractedScene, 0.5);

    // ------------- Beer-Lambert吸収 -----------------------------------------
    // 流体を通る光は exp(-coeff * thickness)に従って波長を失う。
    // 流体の色合い自体は吸収された光に比例して追加される
    // (単一散乱近似)
    vec3 attenuation = exp(-ABSORPTION_COEFF * thickness);
    vec3 transmission = refracted * attenuation + FLUID_TINT * (1.0 - attenuation);

    // ------------- 反射 + Fresnel ------------------------------------------
    vec3 reflectDir = reflect(-V, N);
    vec3 reflected = envSample(reflectDir);

    float NdotV = clamp(dot(N, V), 0.0, 1.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // ------------- 鏡面ハイライト (Blinn-Phong) ----------------------------
    vec3 H = normalize(LIGHT_DIR + V);
    float spec = pow(max(dot(N, H), 0.0), SHININESS);

    vec3 color = mix(transmission, reflected, fresnel) + spec * vec3(1.2);

    FragColor = vec4(color, 1.0);
}
