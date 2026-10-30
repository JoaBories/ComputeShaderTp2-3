#include "scene.hpp"
#include <cstdlib>
namespace {
const float kBox = 64.f;
float rnd() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
}
float boxSize() { return kBox; }
void initParticles(std::vector<Particle>& dst, int n, float L) {
  dst.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    Particle& p = dst[static_cast<size_t>(i)];
    p.posType[0] = rnd() * L; p.posType[1] = rnd() * L; p.posType[2] = rnd() * L;
    p.posType[3] = (i % 2 == 0) ? 1.f : -1.f;
    p.vel[0] = (rnd() - 0.5f) * 2.f; p.vel[1] = (rnd() - 0.5f) * 2.f;
    p.vel[2] = (rnd() - 0.5f) * 2.f; p.vel[3] = 0.f;
  }
}
void initBoids(std::vector<Particle>& dst, int n, float L) {
  initParticles(dst, n, L);
  for (size_t i = 0; i < dst.size(); ++i) dst[i].posType[3] = 0.f;
}
