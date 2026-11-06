#include "scene.hpp"
#include <cstdlib>
namespace {
const float kBox = 1.f;
float rnd() { return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX); }
}
float boxSize() { return kBox; }
void initParticles(std::vector<Particle>& dst, int n, float L) {
  dst.resize(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    Particle& p = dst[static_cast<size_t>(i)];
    float x, y, z;
    float radius = 2.0f;
    do {
      x = (rnd() * 2.f - 1.f) * radius;
      y = (rnd() * 2.f - 1.f) * radius;
      z = (rnd() * 2.f - 1.f) * radius;
    } while (x * x + y * y + z * z > radius * radius);
    
    p.posType[0] = x;
    p.posType[1] = y;
    p.posType[2] = z;
    
    p.posType[3] = (i % 2 == 0) ? 1.f : -1.f;
    
    p.vel[0] = 0.0f;//(rnd() - 0.5f) * 2.f; 
    p.vel[1] = 0.0f;//(rnd() - 0.5f) * 2.f;
    p.vel[2] = 0.0f;//(rnd() - 0.5f) * 2.f; 
    p.vel[3] = 0.0f;
  }
}
void initBoids(std::vector<Particle>& dst, int n, float L) {
  initParticles(dst, n, L);
  for (size_t i = 0; i < dst.size(); ++i) dst[i].posType[3] = 0.f;
}
