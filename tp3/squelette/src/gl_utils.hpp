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
bool loadMap(const char* path);
bool windowShouldClose();
void pollAndSwap();
void shutdownWindow();
int windowWidth();
int windowHeight();

void renderGBuffer();
void displayTexture(GLuint texture);
void displayPreview();
GLuint gbufferPosition();
GLuint gbufferNormal();
GLuint gbufferAlbedo();
GLuint gbufferDepth();

const glm::mat4& viewMatrix();
const glm::mat4& projMatrix();
float zNear();
float zFar();

// Profondeur positive d = -z (repere camera) a partir du z fenetre [0, 1].
float positiveDepthFromWindowZ(float windowZ);
