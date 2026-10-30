#pragma once
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <GL/glew.h>
#define GLFW_INCLUDE_NONE
inline void initGLLoader() {
  glewExperimental = GL_TRUE;
  (void)glewInit();
}
#else
#include <epoxy/gl.h>
inline void initGLLoader() {}
#endif
#include <glm/mat4x4.hpp>
#include <cstdint>
GLuint makeSSBO(GLuint binding, GLsizeiptr bytes, const void* data = 0);
void readSSBO(GLuint buffer, GLsizeiptr bytes, void* dst);
void clearSSBO(GLuint buffer, uint32_t value);
GLuint ceilDiv(GLuint n, GLuint d);
GLuint loadComputeProgram(const char* path);
bool initWindow(int width, int height, const char* title);
bool windowShouldClose();
void pollAndSwap();
void shutdownWindow();
int windowWidth();
int windowHeight();
const glm::mat4& viewMatrix();
const glm::mat4& projMatrix();
void drawPoints(GLuint particleBuffer, GLsizei count);
