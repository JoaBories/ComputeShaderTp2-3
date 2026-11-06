#pragma once

#include <string>
#include <vector>

// Triangles in Y-up (Source Z-up converted). Positions and normals are parallel arrays of floats (xyz).
bool readSourceBsp(const std::string& path, std::vector<float>& positions, std::vector<float>& normals, std::string& error);
bool readSourceBspBuffer(const std::vector<unsigned char>& file, std::vector<float>& positions, std::vector<float>& normals, std::string& error);
bool extractBspFromPk3(const std::string& path, std::vector<unsigned char>& bsp, std::string& error);

struct LevelSurface {
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> uvs;
  std::vector<unsigned char> rgba;
  int width;
  int height;
};

bool readQuakePk3(const std::string& path, const std::vector<unsigned char>& bsp, std::vector<LevelSurface>& surfaces, std::string& error);