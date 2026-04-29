#version 430 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D tex;

void main() {
	float val = texture(tex, TexCoord).r;
//	if (val >= 1.0) {
//		// 背景（深度1.0または無データ）は黒で塗りつぶす
//		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
//	} else {
//		// 深度のわずかな差を強調して表示（真っ白になるのを防ぐ）
//		float linearColor = (1.0 - val) * 20.0;
//
//		FragColor = vec4(vec3(linearColor), 1.0);
//	}
	FragColor = vec4(vec3(val), 1.0);
}