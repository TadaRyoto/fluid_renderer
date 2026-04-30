#version 430 core
// Bilateral filter (separable). Smooths the screen-space depth while preserving
// silhouette edges. Used iteratively in a horizontal/vertical ping-pong.
out float FragColor;
in vec2 TexCoord;

uniform sampler2D srcTex;     // 1st iteration: depthTex (GL_DEPTH_COMPONENT32)
                              // later iterations: previous smoothTex (R32F)
uniform vec2 texelSize;       // (1/width, 1/height)
uniform vec2 blurDir;         // (1,0) horizontal, (0,1) vertical
uniform float blurScale;      // spatial sigma scale
uniform float blurDepthFalloff; // range sigma scale (depth difference sensitivity)
uniform int filterRadius;     // taps per side

void main() {
    float depth = texture(srcTex, TexCoord).r;

    // Background pixels (no fluid). Pass through and skip filtering.
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

        // Skip background neighbors so silhouettes do not bleed inward.
        if (sampleDepth >= 1.0) continue;

        // Spatial Gaussian (distance to center sample).
        float r = fi * blurScale;
        float ws = exp(-r * r);

        // Range Gaussian (depth-difference). This is the "bilateral" part.
        float rd = (sampleDepth - depth) * blurDepthFalloff;
        float wr = exp(-rd * rd);

        float w = ws * wr;
        sum  += sampleDepth * w;
        wsum += w;
    }

    FragColor = (wsum > 0.0) ? (sum / wsum) : depth;
}
