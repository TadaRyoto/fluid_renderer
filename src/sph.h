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

// CPU-based SPH solver (Muller 2003 formulation).
// Neighbour search uses a spatial hash grid for O(N) performance.
class SPHSystem {
public:
    SPHSystem();

    // Fill a gridSize^3 lattice of particles centred at 'center'.
    void initGrid(int gridSize, float spacing, const glm::vec3& center);

    // Advance the simulation by dt seconds (internally split into fixed substeps).
    void step(float dt);

    const std::vector<SPHParticle>& particles() const { return particles_; }
    std::size_t count() const { return particles_.size(); }

    // Containment AABB: particles reflect off these walls with damping.
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

    // SPH parameters
    float h_;             // smoothing radius
    float h2_;            // h^2 (cached)
    float mass_;          // particle mass
    float restDensity_;   // rest density (rho_0)
    float gasConstant_;   // stiffness k in p = k(rho - rho_0)
    float viscosity_;     // dynamic viscosity mu
    glm::vec3 gravity_;
    float boundaryDamping_;

    // Kernel coefficients precomputed from h
    float poly6Coef_;
    float spikyGradCoef_;
    float viscLapCoef_;

    // Fixed substeps per frame
    int substeps_;
};
