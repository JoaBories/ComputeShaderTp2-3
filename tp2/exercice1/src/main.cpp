#include <algorithm>
#include <chrono>

#include "gl_utils.hpp"
#include "scene.hpp"
#include <vector>
#include <GLFW/glfw3.h>
int main() {
  if (!initWindow(1000, 1000, "TP2 exercice 1")) return 1;
  const int N = 131072;
  std::vector<Particle> state;
  initParticles(state, N, boxSize());
  
  const float rS = 1.0f;
  const float rA = 1.6f;
  const float rC = 4.0f;

  const float vMax = 8.0f;
  
  const float R = std::max({ rS, rA, rC });

  const int gridSize = (int)ceil(boxSize() / R);
  const int nC = gridSize * gridSize * gridSize;
  
  
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), state.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), nullptr);
  GLuint CellHeads = makeSSBO(4, static_cast<GLsizeiptr>(nC * sizeof(int)), nullptr);
  GLuint ChainedLists = makeSSBO(5, static_cast<GLsizeiptr>(N * sizeof(int)), nullptr);
  GLuint Forces = makeSSBO(6, static_cast<GLsizeiptr>(N * sizeof(float) * 4), nullptr);
  
  GLuint build = loadComputeProgram("boids_build.comp");
  GLuint forces = loadComputeProgram("boids_forces.comp");
  GLuint integrate = loadComputeProgram("boids_integrate.comp");
  
  double dt = 0.0;
  double lastTime = 0.0;
  
  while (!windowShouldClose()) {
    const double time = glfwGetTime();
    dt = time - lastTime;
    lastTime = time;
    
    clearSSBO(CellHeads, -1);
    
    glUseProgram(build);
    glUniform1ui(glGetUniformLocation(build, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(build, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(build, "cellSize"), R);
    glUniform1i(glGetUniformLocation(build, "nC"), nC);
    glUniform1i(glGetUniformLocation(build, "gridSize"), gridSize);
    glDispatchCompute(ceilDiv(N, 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(forces);
    glUniform1ui(glGetUniformLocation(forces, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(forces, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(forces, "cellSize"), R);
    glUniform1i(glGetUniformLocation(forces, "nC"), nC);
    glUniform1i(glGetUniformLocation(forces, "gridSize"), gridSize);
    glUniform3f(glGetUniformLocation(forces, "r"), rS, rA, rC);
    glDispatchCompute(ceilDiv(N, 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(integrate);
    glUniform1ui(glGetUniformLocation(integrate, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(integrate, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(integrate, "vMax"), vMax);
    glUniform1f(glGetUniformLocation(integrate, "dt"), dt);
    glDispatchCompute(ceilDiv(N, 256), 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    
    drawPoints(next, N);
    GLuint tmp = current;
    current = next;
    next = tmp;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, current);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, next);
    pollAndSwap();
  }
  
  shutdownWindow();
  return 0;
}
