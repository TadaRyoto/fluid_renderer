#version 430 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D tex;

void main() {
	float val = texture(tex, TexCoord).r;
//	if (val >= 1.0) {
//		// 背景(値1.0)は黒く返す
//		FragColor = vec4(0.0, 0.0, 0.0, 1.0);
//	} else {
//		// わずかな色差を強調して表示
//		float linearColor = (1.0 - val) * 20.0;
//
//		FragColor = vec4(vec3(linearColor), 1.0);
//	}
	FragColor = vec4(vec3(val), 1.0);
}
