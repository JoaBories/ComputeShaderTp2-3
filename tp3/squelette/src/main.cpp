#include "gl_utils.hpp"
#include "scene.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>
#include <array>
#include <iostream>

void cullLights(GLuint visSSBO, GLuint shader, GLuint lightSSBO)
{
  clearSSBO(visSSBO, 0xFFFFFFFF);
  
  const uint32_t num_light = (uint32_t)lights().size();
  
  std::array<glm::vec4, 6> planes;

  const glm::vec4 r1 = projMatrix()[0];
  const glm::vec4 r2 = projMatrix()[1];
  const glm::vec4 r3 = projMatrix()[2];
  const glm::vec4 r4 = projMatrix()[3];
  
  planes[0] = r4 + r1;
  planes[1] = r4 - r1;
  planes[2] = r4 + r2;
  planes[3] = r4 - r2;
  planes[4] = r4 + r3;
  planes[5] = r4 - r3;
  
  for (auto& plane : planes)
  {
    const float len = glm::length(plane);
    plane /= len;
  }

  const float plane[6*4] = {
    planes[0].x, planes[0].y, planes[0].z, planes[0].w,
    planes[1].x, planes[1].y, planes[1].z, planes[1].w,
    planes[2].x, planes[2].y, planes[2].z, planes[2].w,
    planes[3].x, planes[3].y, planes[3].z, planes[3].w,
    planes[4].x, planes[4].y, planes[4].z, planes[4].w,
    planes[5].x, planes[5].y, planes[5].z, planes[5].w
  };
  
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  glUseProgram(shader);
  
  glUniform1ui(glGetUniformLocation(shader, "numLights"), num_light);
  glUniform4fv(glGetUniformLocation(shader, "planes"), 6, plane);
  glDispatchCompute(ceilDiv(num_light, 256), 1, 1);
  
  int temp;
  readSSBO(visSSBO, sizeof(int), &temp);
  (void)temp;
  std::cout << temp << "\n";
}

int main(int argc, char** argv) {
  const char* map = "map/1smallmap.pk3";
  
  if (!initWindow(1280, 720, "Compute shaders")) {
    return 1;
  }
  if (!loadMap(map)) {
    return 1;
  }
  
  GLuint visibility = makeSSBO(1, (GLsizeiptr)(lights().size()+1) * sizeof(int), nullptr);
  GLuint cullShader = loadComputeProgram("lightCulling.comp");

  GLuint lightSSBO = 0;
  
  while (!windowShouldClose()) {
    updateLights(static_cast<float>(glfwGetTime()));
    lightSSBO = makeSSBO(0, (GLsizeiptr)(lights().size() * 8 * sizeof(float)), lights().data());
    cullLights(visibility, cullShader, lightSSBO);
    
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    renderGBuffer(lightSSBO);
    displayPreview();
    pollAndSwap();
  }
  
  glDeleteBuffers(1, &visibility);
  glDeleteBuffers(1, &lightSSBO);
  glDeleteProgram(cullShader);
  
  shutdownWindow();
  return 0;
}
