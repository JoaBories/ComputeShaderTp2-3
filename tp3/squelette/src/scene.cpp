#include "scene.hpp"

#include <cmath>
#include <cstdlib>

namespace {
const float kBox = 64.f;
std::vector<PointLight> gLights;

float rnd() {
  return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}
}

const std::vector<PointLight>& lights() { return gLights; }

void updateLights(float timeSeconds) {
  const int n = 10000;
  if (static_cast<int>(gLights.size()) != n) {
    gLights.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
      PointLight& L = gLights[static_cast<size_t>(i)];
      L.positionRadius[0] = rnd() * kBox;
      L.positionRadius[1] = rnd() * kBox;
      L.positionRadius[2] = rnd() * kBox;
      L.positionRadius[3] = 4.f + rnd() * 6.f;
      L.colorIntensity[0] = 0.3f + rnd() * 0.7f;
      L.colorIntensity[1] = 0.3f + rnd() * 0.7f;
      L.colorIntensity[2] = 0.3f + rnd() * 0.7f;
      L.colorIntensity[3] = 1.f;
    }
  }
  for (size_t i = 0; i < gLights.size(); ++i) {
    PointLight& L = gLights[i];
    const float phase = static_cast<float>(i) * 0.17f;
    L.positionRadius[0] += 0.15f * std::sin(timeSeconds + phase);
    L.positionRadius[1] += 0.15f * std::cos(timeSeconds * 0.7f + phase);
  }
}
