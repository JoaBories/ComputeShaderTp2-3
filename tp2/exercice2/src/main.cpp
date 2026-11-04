#include "gl_utils.hpp"
#include "scene.hpp"
#include <vector>
int main() {
  if (!initWindow(800, 800, "TP2 exercice 2")) return 1;
  const int N = 1024;
  std::vector<Particle> state;
  initParticles(state, N, boxSize());
  GLuint current = makeSSBO(0, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), state.data());
  GLuint next = makeSSBO(1, static_cast<GLsizeiptr>(state.size() * sizeof(Particle)), 0);
  GLuint prog = loadComputeProgram("particles.comp");
  while (!windowShouldClose()) {
    glUseProgram(prog);
    glUniform1ui(glGetUniformLocation(prog, "N"), static_cast<GLuint>(N));
    glUniform1f(glGetUniformLocation(prog, "boxSize"), boxSize());
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
