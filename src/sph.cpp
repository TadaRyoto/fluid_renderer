#include "sph.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace {

inline glm::ivec3 cellOf(const glm::vec3& p, float h) {
    return glm::ivec3(
        static_cast<int>(std::floor(p.x / h)),
        static_cast<int>(std::floor(p.y / h)),
        static_cast<int>(std::floor(p.z / h))
    );
}

// ivec3セルインデックスを64ビットキーにパック。各軸は±2^20に収まる。
inline long long hashCell(const glm::ivec3& c) {
    constexpr long long bias = 1LL << 20;
    return ((static_cast<long long>(c.x) + bias) & 0x1FFFFF)
         | (((static_cast<long long>(c.y) + bias) & 0x1FFFFF) << 21)
         | (((static_cast<long long>(c.z) + bias) & 0x1FFFFF) << 42);
}

}  // namespace

SPHSystem::SPHSystem()
    : h_(0.13f),
	  spacing_(0.08f),
      // 質量は初期の立方格子配置（spacing 0.08, h 0.13）がPoly6密度で
      // ほぼrestDensity_を生成するよう調整→初期圧力≒0、
      // 床で圧力が高まる前に重力がクラスターを引き下げられるようにする。
      mass_(0.32f),
      restDensity_(600.0f),
      gasConstant_(2000.0f),
      gasExponent_(3.0f),
      // 低粘性により運動エネルギーが保たれ、飛沫が動き続ける。
      viscosity_(0.15f),
      gravity_(0.0f, -9.8f, 0.0f),
      // -0.3で壁衝突時の法線速度を30%保持（運動エネルギーの9%）→反発の少ない壁。
      boundaryDamping_(-0.3f),
      substeps_(2) {
    h2_ = h_ * h_;
    constexpr float pi = glm::pi<float>();
    poly6Coef_     =  315.0f / (64.0f * pi * std::powf(h_, 9.0f));
    spikyGradCoef_ = -45.0f  / (pi * std::powf(h_, 6.0f));
    viscLapCoef_   =  45.0f  / (pi * std::powf(h_, 6.0f));
}

void SPHSystem::initGrid(int gridSize, const glm::vec3& center) {
    particles_.clear();
    particles_.reserve(static_cast<std::size_t>(gridSize) * gridSize * gridSize);

    const float offset = (gridSize - 1) * spacing_ * 0.5f;

    // 位置に小さな擾乱を加えて対称性を崩す。
    std::mt19937 rng(1234u);
    const float jitter = spacing_ * 0.05f;
    std::uniform_real_distribution<float> dist(-jitter, jitter);

    for (int x = 0; x < gridSize; ++x) {
        for (int y = 0; y < gridSize; ++y) {
            for (int z = 0; z < gridSize; ++z) {
                SPHParticle p{};
                p.position = center + glm::vec3(
                    x * spacing_ - offset + dist(rng),
                    y * spacing_ - offset + dist(rng),
                    z * spacing_ - offset + dist(rng));
                p.velocity = glm::vec3(0.0f);
                p.force    = glm::vec3(0.0f);
                p.density  = restDensity_;
                p.pressure = 0.0f;
                particles_.push_back(p);
            }
        }
    }

    // 境界AABB：
    // - x/zは初期キューブの約2倍に広げ、流体が側壁に押しつけられる
    //   代わりに水たまりに広がれるようにする。
    // - yはキューブ下方に落下距離を確保し、
    //   上方向の飛沫用に小さなマージンを設ける。
    const float lateralMargin = offset * 1.0f + spacing_ * 2.0f; // キューブ幅の約2倍
    const float floorMargin   = spacing_ * 6.0f;                 // 約0.5mの落下空間
    const float ceilingMargin = spacing_ * 2.0f;
    boundsMin = center + glm::vec3(-(offset + lateralMargin),
                                   -(offset + floorMargin),
                                   -(offset + lateralMargin));
    boundsMax = center + glm::vec3(  offset + lateralMargin,
                                     offset + ceilingMargin,
                                     offset + lateralMargin);
}

void SPHSystem::buildGrid() {
    grid_.clear();
    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        const long long key = hashCell(cellOf(particles_[i].position, h_));
        grid_[key].push_back(i);
    }
}

void SPHSystem::computeDensityPressure() {
    const int n = static_cast<int>(particles_.size());
    for (int i = 0; i < n; ++i) {
        const glm::ivec3 c = cellOf(particles_[i].position, h_);
        float density = 0.0f;
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    const auto it = grid_.find(hashCell(c + glm::ivec3(dx, dy, dz)));
                    if (it == grid_.end()) continue;
                    for (int j : it->second) {
                        const glm::vec3 r = particles_[j].position - particles_[i].position;
                        const float r2 = glm::dot(r, r);
                        if (r2 < h2_) {
                            const float diff = h2_ - r2;
                            density += mass_ * poly6Coef_ * diff * diff * diff;
                        }
                    }
                }
            }
        }
        particles_[i].density  = density;
        particles_[i].pressure = std::max(gasConstant_ * (std::powf(density / restDensity_, gasExponent_) - 1.0f), 0.0f);
    }
}

void SPHSystem::computeForces() {
    const int n = static_cast<int>(particles_.size());
    for (int i = 0; i < n; ++i) {
        glm::vec3 fp(0.0f);
        glm::vec3 fv(0.0f);
        const glm::ivec3 c = cellOf(particles_[i].position, h_);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    const auto it = grid_.find(hashCell(c + glm::ivec3(dx, dy, dz)));
                    if (it == grid_.end()) continue;
                    for (int j : it->second) {
                        if (j == i) continue;
                        const glm::vec3 r = particles_[i].position - particles_[j].position;
                        const float dist = glm::length(r);
                        if (dist > 0.0f && dist < h_) {
                            const glm::vec3 dir = r / dist;
                            const float gap = h_ - dist;
                            // 圧力力
                            fp += dir * (-mass_ * mass_
                                * (particles_[i].pressure / (particles_[i].density * particles_[i].density)
                                    + particles_[j].pressure / (particles_[j].density * particles_[j].density))
                                * spikyGradCoef_ * gap * gap);
                            // 粘性力
                            fv += viscosity_ * mass_ * mass_
                                * (particles_[j].velocity - particles_[i].velocity)
                                / particles_[j].density
                                * viscLapCoef_ * gap;
                        }
                    }
                }
            }
        }
        const glm::vec3 fg = gravity_ * mass_;
        particles_[i].force = fp + fv + fg;
    }
}

void SPHSystem::integrate(float dt) {
    for (auto& p : particles_) {
        if (p.density <= 0.0f) continue;
        const glm::vec3 a = p.force / mass_;
        p.velocity += a * dt;
        p.position += p.velocity * dt;
    }
}

void SPHSystem::enforceBounds() {
    for (auto& p : particles_) {
        for (int axis = 0; axis < 3; ++axis) {
            if (p.position[axis] < boundsMin[axis]) {
                p.position[axis] = boundsMin[axis];
                p.velocity[axis] *= boundaryDamping_;
            } else if (p.position[axis] > boundsMax[axis]) {
                p.position[axis] = boundsMax[axis];
                p.velocity[axis] *= boundaryDamping_;
            }
        }
    }
}

void SPHSystem::step(float dt) {
    if (particles_.empty() || dt <= 0.0f) return;

    // 大きなフレーム間隔での発散を防ぐためdtをクランプ。
    dt = std::min(dt, 1.0f / 30.0f);
    const float sub_dt = dt / static_cast<float>(substeps_);

    for (int s = 0; s < substeps_; ++s) {
        buildGrid();
        computeDensityPressure();
        computeForces();
        integrate(sub_dt);
        enforceBounds();
    }
}
