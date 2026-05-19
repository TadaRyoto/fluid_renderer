#pragma once

#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

struct SPHParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 force;
    float density;
    float pressure;
};

// CPUベースのSPHソルバー（Muller 2003方式）。
// 近傍探索にはO(N)性能の空間ハッシュグリッドを使用。
class SPHSystem {
public:
    SPHSystem();

    // 'center'を中心としたgridSize^3格子状に粒子を配置する。
    void initGrid(int gridSize, const glm::vec3& center);

    // シミュレーションをdt秒進める（内部で固定サブステップに分割）。
    void step(float dt);

    const std::vector<SPHParticle>& particles() const { return particles_; }
    std::size_t count() const { return particles_.size(); }

    // 境界AABB：粒子はこれらの壁で減衰しながら反射する。
    glm::vec3 boundsMin{};
    glm::vec3 boundsMax{};

private:
    void buildGrid();
    void computeDensityPressure();
    void computeForces();
    void integrate(float dt);
    void enforceBounds();

    std::vector<SPHParticle> particles_;
    std::unordered_map<long long, std::vector<int>> grid_;

    // SPHパラメータ
    float h_;             // 影響半径
    float h2_;            // h^2（キャッシュ）
    float spacing_;       // グリッドの幅
    float mass_;          // 粒子質量
    float restDensity_;   // 静止密度（rho_0）
    float gasConstant_;   // 圧力計算の剛性係数k
	float gasExponent_;   // 圧力計算の指数
    float viscosity_;     // 動粘性係数mu
    glm::vec3 gravity_;
    float boundaryDamping_;

    // hから事前計算されたカーネル係数
    float poly6Coef_;
    float spikyGradCoef_;
    float viscLapCoef_;

    // 1フレームあたりの固定サブステップ数
    int substeps_;
};
