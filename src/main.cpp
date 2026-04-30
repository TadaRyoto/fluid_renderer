#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <windows.h>

#include "sph.h"

static std::string g_assetsDir;

static void initAssetsDir() {
    char buf[MAX_PATH];
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string exePath(buf);
    g_assetsDir = exePath.substr(0, exePath.find_last_of("\\/")) + "/assets/";
}

constexpr unsigned int SCR_WIDTH = 800;
constexpr unsigned int SCR_HEIGHT = 600;

GLuint depthFBO, depthTex;
GLuint thicknessFBO, thicknessTex;
GLuint smoothFBO[2], smoothTex[2];
GLuint normalFBO, normalTex;

GLuint quadVAO, quadVBO;
GLuint particleVAO, particleVBO;
int particleCount = 0;

static SPHSystem sph;

GLuint depthProgram, thicknessProgram, debugProgram;
GLuint smoothProgram, normalProgram, compositeProgram;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    return shader;
}

GLuint createProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, NULL, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

std::string readShaderSource(const char* filePath) {
    std::string shaderCode;
    std::ifstream shaderFile;

    shaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        shaderFile.open(filePath);
        std::stringstream shaderStream;
        shaderStream << shaderFile.rdbuf();
        shaderFile.close();
        shaderCode = shaderStream.str();
    } catch (std::ifstream::failure& e) {
        std::cerr << "Error reading shader file: " << filePath << std::endl;
    }

    return shaderCode;
}

void setupShaders() {
	std::string particleSource = readShaderSource((g_assetsDir + "shaders/particle.vert").c_str());
	const char* particleVS = particleSource.c_str();
	std::string depthSource = readShaderSource((g_assetsDir + "shaders/depth.frag").c_str());
	const char* depthFS = depthSource.c_str();
	std::string thicknessSource = readShaderSource((g_assetsDir + "shaders/thickness.frag").c_str());
	const char* thicknessFS = thicknessSource.c_str();
	std::string quadSource = readShaderSource((g_assetsDir + "shaders/quad.vert").c_str());
	const char* quadVS = quadSource.c_str();
	std::string debugSource = readShaderSource((g_assetsDir + "shaders/debug.frag").c_str());
	const char* debugFS = debugSource.c_str();
	std::string smoothSource = readShaderSource((g_assetsDir + "shaders/smooth.frag").c_str());
	const char* smoothFS = smoothSource.c_str();
	std::string normalSource = readShaderSource((g_assetsDir + "shaders/normal.frag").c_str());
	const char* normalFS = normalSource.c_str();
	std::string compositeSource = readShaderSource((g_assetsDir + "shaders/composite.frag").c_str());
	const char* compositeFS = compositeSource.c_str();
    depthProgram = createProgram(particleVS, depthFS);
    thicknessProgram = createProgram(particleVS, thicknessFS);
    debugProgram = createProgram(quadVS, debugFS);
    smoothProgram = createProgram(quadVS, smoothFS);
    normalProgram = createProgram(quadVS, normalFS);
    compositeProgram = createProgram(quadVS, compositeFS);
}

void createFramebuffers(GLuint &FBO, GLuint &tex, GLenum internalFormat, GLenum format, GLenum attachment) {
    glGenFramebuffers(1, &FBO);

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, SCR_WIDTH, SCR_HEIGHT, 0, format, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, tex, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer not complete" << std::endl;
    }
}

void setupFBOs() {
    // depthマップ用FBO
	createFramebuffers(depthFBO, depthTex, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_DEPTH_ATTACHMENT);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

	// thicknessマップ用FBO
	createFramebuffers(thicknessFBO, thicknessTex, GL_R32F, GL_RED, GL_COLOR_ATTACHMENT0);

    // 平滑化用FBO
    for (unsigned int i = 0; i < 2; i++) {
		createFramebuffers(smoothFBO[i], smoothTex[i], GL_R32F, GL_RED, GL_COLOR_ATTACHMENT0);
    }

    // normalマップ用FBO
	createFramebuffers(normalFBO, normalTex, GL_RGB16F, GL_RGB, GL_COLOR_ATTACHMENT0);

    // デフォルトのフレームバッファに戻す
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void setupQuad() {
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void setupParticles() {
    // Init SPH: 10^3 = 1000 particles centred at (0, 0.5, -2)
    constexpr int   gridSize = 10;
    constexpr float spacing  = 0.08f;
    const glm::vec3 center(0.0f, 0.5f, -2.0f);
    sph.initGrid(gridSize, spacing, center);
    particleCount = static_cast<int>(sph.count());

    glGenVertexArrays(1, &particleVAO);
    glGenBuffers(1, &particleVBO);
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    // GL_DYNAMIC_DRAW hints that this buffer will be rewritten every frame.
    glBufferData(GL_ARRAY_BUFFER,
                 particleCount * static_cast<GLsizeiptr>(sizeof(glm::vec3)),
                 nullptr,
                 GL_DYNAMIC_DRAW);

    // Location 0: vec3 position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glBindVertexArray(0);

    // Upload initial positions so the first frame renders correctly.
    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);
    const auto& ptcls = sph.particles();
    std::vector<glm::vec3> initPos(ptcls.size());
    for (std::size_t i = 0; i < ptcls.size(); ++i)
        initPos[i] = ptcls[i].position;
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(initPos.size() * sizeof(glm::vec3)),
                    initPos.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Stream SPH positions into the VBO every frame.
// GL_MAP_INVALIDATE_BUFFER_BIT orphans the old buffer so the GPU does not stall
// waiting for a previous draw to finish (equivalent to the "buffer orphaning" trick
// with glBufferData(NULL) + glBufferSubData, but in a single call).
void updateParticleVBO() {
    const auto& ptcls = sph.particles();
    const GLsizeiptr bytes =
        static_cast<GLsizeiptr>(ptcls.size() * sizeof(glm::vec3));

    glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

    glm::vec3* dst = reinterpret_cast<glm::vec3*>(
        glMapBufferRange(GL_ARRAY_BUFFER, 0, bytes,
                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    if (dst) {
        for (std::size_t i = 0; i < ptcls.size(); ++i)
            dst[i] = ptcls[i].position;
        glUnmapBuffer(GL_ARRAY_BUFFER);
    } else {
        // Fallback: glBufferSubData (allocates a temporary host buffer)
        std::vector<glm::vec3> pos(ptcls.size());
        for (std::size_t i = 0; i < ptcls.size(); ++i)
            pos[i] = ptcls[i].position;
        glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, pos.data());
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void renderQuad() {
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void renderFluidPipeline() {
	glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
	glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -4.0f));

	float pointRadius = 0.15f;

	// depthマップの生成
    glBindFramebuffer(GL_FRAMEBUFFER, depthFBO);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    // 深度計算用シェーダの使用
    glUseProgram(depthProgram);
    glUniformMatrix4fv(glGetUniformLocation(depthProgram, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(depthProgram, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(depthProgram, "pointRadius"), pointRadius);
    glUniform1f(glGetUniformLocation(depthProgram, "screenHeight"), (float)SCR_HEIGHT);

    // パーティクルの描画（ポイントスプライト）
	glBindVertexArray(particleVAO);
	glDrawArrays(GL_POINTS, 0, particleCount);
	glBindVertexArray(0);

    // thicknessマップの生成
    glBindFramebuffer(GL_FRAMEBUFFER, thicknessFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

	// 厚み計算用シェーダの使用
    glUseProgram(thicknessProgram);
    glUniformMatrix4fv(glGetUniformLocation(thicknessProgram, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(thicknessProgram, "projectionMatrix"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1f(glGetUniformLocation(thicknessProgram, "pointRadius"), pointRadius);
    glUniform1f(glGetUniformLocation(thicknessProgram, "screenHeight"), (float)SCR_HEIGHT);

    // 厚み計算用パーティクルの描画処理
    glBindVertexArray(particleVAO);
    glDrawArrays(GL_POINTS, 0, particleCount);
    glBindVertexArray(0);

    glDisable(GL_BLEND);

	// depthマップの平滑化 (双方向セパラブル・バイラテラルフィルタ)
    bool horizontal = true, first_iteration = true;
    constexpr int amount = 30; // 平滑化の反復回数

    glUseProgram(smoothProgram);
    glUniform1i(glGetUniformLocation(smoothProgram, "srcTex"), 0);
    glUniform2f(glGetUniformLocation(smoothProgram, "texelSize"),
                1.0f / (float)SCR_WIDTH, 1.0f / (float)SCR_HEIGHT);
    // 空間ガウシアン σ ~= 1/blurScale (スケールが大きいほど鋭いカーネル)
    glUniform1f(glGetUniformLocation(smoothProgram, "blurScale"), 0.18f);
    // 深度差の許容度。値が大きいほど隣接深度の差に敏感 (=エッジ保持が強い)
    glUniform1f(glGetUniformLocation(smoothProgram, "blurDepthFalloff"), 250.0f);
    glUniform1i(glGetUniformLocation(smoothProgram, "filterRadius"), 8);

    for (unsigned int i = 0; i < amount; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, smoothFBO[horizontal]);
        glClear(GL_COLOR_BUFFER_BIT);

        // 反復ごとに H/V を切り替え (セパラブルフィルタ)
        glUniform2f(glGetUniformLocation(smoothProgram, "blurDir"),
                    horizontal ? 1.0f : 0.0f,
                    horizontal ? 0.0f : 1.0f);

        // 最初の反復では深度マップを、以降はPing-Pongテクスチャを読み込む
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? depthTex : smoothTex[!horizontal]);

        renderQuad();
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    // 平滑化結果が入っているテクスチャ (ループ末尾の glBindFramebuffer 対象は smoothFBO[!horizontal])
    GLuint smoothedDepthTex = smoothTex[!horizontal];

    // 投影行列の逆 (法線再構築 / 最終合成で view 空間へ戻すために使用)
    glm::mat4 invProjection = glm::inverse(projection);

    // normalマップの生成 (平滑化された深度から view 空間法線を再構築)
    glBindFramebuffer(GL_FRAMEBUFFER, normalFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(normalProgram);
    glUniform1i(glGetUniformLocation(normalProgram, "depthTex"), 0);
    glUniform2f(glGetUniformLocation(normalProgram, "texelSize"),
                1.0f / (float)SCR_WIDTH, 1.0f / (float)SCR_HEIGHT);
    glUniformMatrix4fv(glGetUniformLocation(normalProgram, "invProjectionMatrix"),
                       1, GL_FALSE, glm::value_ptr(invProjection));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smoothedDepthTex);
    renderQuad();

    // 最終合成 (デフォルトフレームバッファへ画面出力)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(compositeProgram);
    glUniform1i(glGetUniformLocation(compositeProgram, "depthTex"), 0);
    glUniform1i(glGetUniformLocation(compositeProgram, "normalTex"), 1);
    glUniform1i(glGetUniformLocation(compositeProgram, "thicknessTex"), 2);
    glUniformMatrix4fv(glGetUniformLocation(compositeProgram, "invProjectionMatrix"),
                       1, GL_FALSE, glm::value_ptr(invProjection));
    glUniform1f(glGetUniformLocation(compositeProgram, "thicknessScale"), 1.0e10f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, smoothedDepthTex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTex);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, thicknessTex);

    renderQuad();
}

int main() {
    initAssetsDir();

	// GLFWの初期化
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // ウィンドウの作成
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "fluid renderer", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// GLADの初期化
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_PROGRAM_POINT_SIZE);

	// シェーダのセットアップ
    setupShaders();

	// FBO、Quad、パーティクルのセットアップ
	setupFBOs();
	setupQuad();
	setupParticles();

    // レンダリングループ
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (!glfwWindowShouldClose(window)) {
        const auto now = std::chrono::high_resolution_clock::now();
        const float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        sph.step(dt);
        updateParticleVBO();
        renderFluidPipeline();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}