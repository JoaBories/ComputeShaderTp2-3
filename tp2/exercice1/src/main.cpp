#include <algorithm>
#include <chrono>

#include "gl_utils.hpp"
#include "scene.hpp"
#include <vector>
int main() {
  if (!initWindow(800, 800, "TP2 exercice 1")) return 1;
  const int N = 262144;
  std::vector<Particle> state;
  initParticles(state, N, boxSize());
  
  const float rS = 1.0f;
  const float rA = 2.0f;
  const float rC = 3.0f;

  const float R = std::max({ rS, rA, rC });

  const int nCc = (int)ceil(boxSize() / R);
  const int nC = nCc * nCc * nCc;
  
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), state.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), nullptr);
  GLuint CellHeads = makeSSBO(4, static_cast<GLsizeiptr>(nC * sizeof(int)), nullptr);
  GLuint ChainedLists = makeSSBO(5, static_cast<GLsizeiptr>(N * sizeof(int)), nullptr);
  
  GLuint build = loadComputeProgram("boids_build.comp");
  GLuint forces = loadComputeProgram("boids_forces.comp");
  GLuint integrate = loadComputeProgram("boids_integrate.comp");
  
  while (!windowShouldClose()) {
    glUseProgram(build);
    glUniform1ui(glGetUniformLocation(build, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(build, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(build, "h"), R);
    glUniform1i(glGetUniformLocation(build, "nC"), nC);
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(forces);
    glUniform1ui(glGetUniformLocation(forces, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(forces, "boxSize"), boxSize());
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(integrate);
    glUniform1ui(glGetUniformLocation(integrate, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(integrate, "boxSize"), boxSize());
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
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
