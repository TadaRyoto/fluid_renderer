#version 430 core
out float FragColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard;

    // 球の半厚さ。重なるポイントスプライト間で加算累積され、
    // スクリーン空間の厚さマップを構築する。出力は意図的に非常に小さい値。
    // コンポジット段階で `thicknessScale` (~1e10)を乗じて値を
    // 有用な範囲に戻しながら、R32Fターゲットが重いオーバーラップで
    // 飽和するのを防ぐ。
    float thickness = sqrt(1.0 - r2);
    FragColor = thickness * 1e-12;
}
