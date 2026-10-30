#include "gl_utils.hpp"
#include "scene.hpp"

#include <GLFW/glfw3.h>

int main() {
  if (!initWindow(1280, 720, "Compute shaders")) {
    return 1;
  }
  while (!windowShouldClose()) {
    updateLights(static_cast<float>(glfwGetTime()));
    renderGBuffer();
    displayTexture(gbufferAlbedo());
    pollAndSwap();
  }
  shutdownWindow();
  return 0;
}
