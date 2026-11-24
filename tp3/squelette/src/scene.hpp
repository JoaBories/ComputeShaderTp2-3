#pragma once

#include <vector>

struct PointLight {
  alignas(16) float positionRadius[4];
  alignas(16) float colorIntensity[4];
};

const std::vector<PointLight>& lights();
size_t nLights();
void updateLights(float timeSeconds);
