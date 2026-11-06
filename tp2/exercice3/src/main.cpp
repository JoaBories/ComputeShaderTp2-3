#include <iostream>
#include <ostream>

#include "gl_utils.hpp"
#include "scene.hpp"
#include <vector>
#include <GLFW/glfw3.h>
int main() {
  if (!initWindow(1000, 1000, "TP2 exercice 3")) return 1;
  const int N = 1024*1024;
  const int gridSize = 32;
  const int cells = gridSize * gridSize * gridSize;
  std::vector<Particle> state;
  initParticles(state, N, boxSize());
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), state.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), 0);
  
  const float R = 6.0f;
  
  int size = static_cast<int>(ceil(boxSize()))/2;
  int max[6] = {-size, -size, -size, size, size, size};
  
  GLuint heads = makeSSBO(4, static_cast<GLsizeiptr>(cells * sizeof(int)), 0);
  makeSSBO(5, static_cast<GLsizeiptr>(N * sizeof(int)), 0);
  GLuint maximums = makeSSBO(6, static_cast<GLsizeiptr>(8 * sizeof(int)), max);
  
  GLuint maxi = loadComputeProgram("build_max.comp");
  GLuint build = loadComputeProgram("build_lists.comp");
  GLuint query = loadComputeProgram("query.comp");
  
  double dt = 0.0;
  double lastTime = glfwGetTime();
  
  while (!windowShouldClose()) {
    const double time = glfwGetTime();
    dt = time - lastTime;
    lastTime = time;
    
    clearSSBO(heads, -1u);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(maxi);
    glUniform1ui(glGetUniformLocation(build, "N"), static_cast<GLuint>(N));
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(build);
    glUniform1ui(glGetUniformLocation(build, "N"), static_cast<GLuint>(N));
    glUniform1i(glGetUniformLocation(build, "gridSize"), static_cast<GLint>(gridSize));
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(query);
    glUniform1ui(glGetUniformLocation(query, "N"), static_cast<GLuint>(N));
    glUniform1i(glGetUniformLocation(query, "gridSize"), static_cast<GLint>(gridSize));
    glUniform1f(glGetUniformLocation(query, "dt"), static_cast<GLfloat>(dt*1e-1f));
    glUniform1f(glGetUniformLocation(query, "R"), R); 
    glDispatchCompute(ceilDiv(static_cast<GLuint>(N), 256), 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
    
    std::vector<int> ssboResult(8, 0);
    readSSBO(maximums, 8*sizeof(int), ssboResult.data());
    float bmin[3] = { (float)ssboResult[0], (float)ssboResult[1], (float)ssboResult[2] };
    float bmax[3] = { (float)ssboResult[4], (float)ssboResult[5], (float)ssboResult[6] };
    
    drawPoints(next, N);
    drawGridImpl(bmin, bmax);
    GLuint tmp = current; current = next; next = tmp;
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, current);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, next);
    pollAndSwap();
  }
  shutdownWindow();
  return 0;
}