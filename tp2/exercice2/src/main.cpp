#include "gl_utils.hpp"
#include "scene.hpp"
#include <vector>
#include <GLFW/glfw3.h>
int main() {
  if (!initWindow(800, 800, "TP2 exercice 2")) return 1;
  const int N = 16384*2;
  std::vector<Particle> state;
  initParticles(state, N, boxSize());
  
  const float R = 6.0f;

  const int gridSize = (int)ceil(boxSize() / R);
  const int nC = gridSize * gridSize * gridSize;
  
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), state.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), 0);
  GLuint CellHeads = makeSSBO(4, static_cast<GLsizeiptr>(nC * sizeof(int)), nullptr);
  GLuint ChainedLists = makeSSBO(5, static_cast<GLsizeiptr>(N * sizeof(int)), nullptr);
  GLuint Forces = makeSSBO(6, static_cast<GLsizeiptr>(N * sizeof(float) * 4), nullptr);
  
  GLuint build = loadComputeProgram("build_lists.comp");
  GLuint prog = loadComputeProgram("particles.comp");
  
  double dt = 0.0;
  double lastTime = glfwGetTime();
  
  while (!windowShouldClose()) {
    const double time = glfwGetTime();
    dt = time - lastTime;
    lastTime = time;
    
    clearSSBO(CellHeads, -1);
    
    glUseProgram(build);
    glUniform1ui(glGetUniformLocation(build, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(build, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(build, "cellSize"), R);
    glUniform1i(glGetUniformLocation(build, "gridSize"), gridSize);
    glUniform1i(glGetUniformLocation(build, "nC"), nC);
    glDispatchCompute(ceilDiv(N, 256), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glUseProgram(prog);
    glUniform1ui(glGetUniformLocation(prog, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(prog, "dt"), static_cast<GLfloat>(dt));
    glUniform1f(glGetUniformLocation(prog, "boxSize"), boxSize());
    glUniform1f(glGetUniformLocation(prog, "R"), R);
    glUniform1i(glGetUniformLocation(prog, "nC"), nC);
    glUniform1i(glGetUniformLocation(prog, "gridSize"), gridSize);
    
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
