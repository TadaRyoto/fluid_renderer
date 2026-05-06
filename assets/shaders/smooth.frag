#version 430 core
// バイラテラルフィルタ(分離可能)。スクリーン空間の深度を平滑化しながら
// シルエットエッジを保持する。水平/垂直ピンポン方式で反復的に使用される。
out float FragColor;
in vec2 TexCoord;

uniform sampler2D srcTex;     // 1回目の反復: depthTex (GL_DEPTH_COMPONENT32)
                              // 以降の反復: 前回のsmoothTex (R32F)
uniform vec2 texelSize;       // (1/width, 1/height)
uniform vec2 blurDir;         // (1,0)水平、(0,1)垂直
uniform float blurScale;      // 空間シグマスケール
uniform float blurDepthFalloff; // 範囲シグマスケール(深度差の感度)
uniform int filterRadius;     // サイド毎のタップ数

void main() {
    float depth = texture(srcTex, TexCoord).r;

    // 背景ピクセル(流体なし)。パススルーしてフィルタリングをスキップ
    if (depth >= 1.0) {
        FragColor = depth;
        return;
    }

    float sum = 0.0;
    float wsum = 0.0;

    for (int i = -filterRadius; i <= filterRadius; ++i) {
        float fi = float(i);
        vec2 offset = blurDir * texelSize * fi;
        float sampleDepth = texture(srcTex, TexCoord + offset).r;

        // シルエットが内側に流れ込まないように、背景の隣接ピクセルをスキップ
        if (sampleDepth >= 1.0) continue;

        // 空間ガウシアン(中心サンプルまでの距離)
        float r = fi * blurScale;
        float ws = exp(-r * r);

        // 範囲ガウシアン(深度差)。これが"バイラテラル"部分
        float rd = (sampleDepth - depth) * blurDepthFalloff;
        float wr = exp(-rd * rd);

        float w = ws * wr;
        sum  += sampleDepth * w;
        wsum += w;
    }

    FragColor = (wsum > 0.0) ? (sum / wsum) : depth;
}
