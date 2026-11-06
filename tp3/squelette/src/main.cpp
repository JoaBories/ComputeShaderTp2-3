#include "gl_utils.hpp"
#include "scene.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
  const char* map = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--maps" && i + 1 < argc) {
      map = argv[++i];
    }
  }
  if (!map) {
    std::fprintf(stderr, "usage: %s --maps fichier.bsp\n", argv[0]);
    return 1;
  }
  if (!initWindow(1280, 720, "Compute shaders")) {
    return 1;
  }
  if (!loadMap(map)) {
    return 1;
  }
  while (!windowShouldClose()) {
    updateLights(static_cast<float>(glfwGetTime()));
    renderGBuffer();
    displayPreview();
    pollAndSwap();
  }
  shutdownWindow();
  return 0;
}
