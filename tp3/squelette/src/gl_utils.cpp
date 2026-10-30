#include "gl_utils.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
GLFWwindow* gWindow = 0;
int gWidth = 1280;
int gHeight = 720;
glm::mat4 gView(1.f);
glm::mat4 gProj(1.f);
const float kNear = 0.1f;
const float kFar = 200.f;

GLuint gFbo = 0;
GLuint gPos = 0;
GLuint gNrm = 0;
GLuint gAlb = 0;
GLuint gDepth = 0;
GLuint gGbufferProg = 0;
GLuint gDisplayProg = 0;
GLuint gCube = 0;
GLuint gQuad = 0;
GLsizei gCubeCount = 0;

std::string readFile(const std::string& path) {
  std::ifstream in(path.c_str());
  if (!in) {
    throw std::runtime_error("lecture impossible : " + path);
  }
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

GLuint compile(GLenum type, const std::string& src, const std::string& name) {
  GLuint shader = glCreateShader(type);
  const char* c = src.c_str();
  glShaderSource(shader, 1, &c, 0);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(shader, sizeof(log), 0, log);
    throw std::runtime_error(name + " : " + log);
  }
  return shader;
}

GLuint linkProgram(GLuint a, GLuint b) {
  GLuint program = glCreateProgram();
  glAttachShader(program, a);
  if (b) {
    glAttachShader(program, b);
  }
  glLinkProgram(program);
  GLint ok = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(program, sizeof(log), 0, log);
    throw std::runtime_error(std::string("link : ") + log);
  }
  glDeleteShader(a);
  if (b) {
    glDeleteShader(b);
  }
  return program;
}

std::string shaderPath(const char* path) {
  std::ifstream direct(path);
  if (direct) {
    return path;
  }
  return std::string(SHADER_DIR) + "/" + path;
}

void makeTargets(int w, int h) {
  if (!gFbo) {
    glGenFramebuffers(1, &gFbo);
    glGenTextures(1, &gPos);
    glGenTextures(1, &gNrm);
    glGenTextures(1, &gAlb);
    glGenTextures(1, &gDepth);
  }
  GLuint colors[3] = {gPos, gNrm, gAlb};
  for (int i = 0; i < 3; ++i) {
    glBindTexture(GL_TEXTURE_2D, colors[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }
  glBindTexture(GL_TEXTURE_2D, gDepth);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gPos, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gNrm, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, gAlb, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);
  const GLenum bufs[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, bufs);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void uploadCube() {
  const float s = 8.f;
  const float o = 28.f;
  const float v[] = {
      o, o, o, 0, 0, -1, o + s, o, o, 0, 0, -1, o + s, o + s, o, 0, 0, -1,
      o, o, o, 0, 0, -1, o + s, o + s, o, 0, 0, -1, o, o + s, o, 0, 0, -1,
      o, o, o + s, 0, 0, 1, o + s, o + s, o + s, 0, 0, 1, o + s, o, o + s, 0, 0, 1,
      o, o, o + s, 0, 0, 1, o, o + s, o + s, 0, 0, 1, o + s, o + s, o + s, 0, 0, 1,
      o, o, o, 0, -1, 0, o + s, o, o + s, 0, -1, 0, o + s, o, o, 0, -1, 0,
      o, o, o, 0, -1, 0, o, o, o + s, 0, -1, 0, o + s, o, o + s, 0, -1, 0,
      o, o + s, o, 0, 1, 0, o + s, o + s, o, 0, 1, 0, o + s, o + s, o + s, 0, 1, 0,
      o, o + s, o, 0, 1, 0, o + s, o + s, o + s, 0, 1, 0, o, o + s, o + s, 0, 1, 0,
      o, o, o, -1, 0, 0, o, o + s, o, -1, 0, 0, o, o + s, o + s, -1, 0, 0,
      o, o, o, -1, 0, 0, o, o + s, o + s, -1, 0, 0, o, o, o + s, -1, 0, 0,
      o + s, o, o, 1, 0, 0, o + s, o + s, o + s, 1, 0, 0, o + s, o + s, o, 1, 0, 0,
      o + s, o, o, 1, 0, 0, o + s, o, o + s, 1, 0, 0, o + s, o + s, o + s, 1, 0, 0,
  };
  gCubeCount = 36;
  glGenVertexArrays(1, &gCube);
  GLuint buf = 0;
  glGenBuffers(1, &buf);
  glBindVertexArray(gCube);
  glBindBuffer(GL_ARRAY_BUFFER, buf);
  glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
  glBindVertexArray(0);
}

void uploadQuad() {
  const float q[] = {-1.f, -1.f, 1.f, -1.f, -1.f, 1.f, -1.f, 1.f, 1.f, -1.f, 1.f, 1.f};
  glGenVertexArrays(1, &gQuad);
  GLuint buf = 0;
  glGenBuffers(1, &buf);
  glBindVertexArray(gQuad);
  glBindBuffer(GL_ARRAY_BUFFER, buf);
  glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
  glBindVertexArray(0);
}

void updateCamera() {
  gView = glm::lookAt(glm::vec3(20.f, 18.f, 90.f), glm::vec3(32.f, 20.f, 32.f), glm::vec3(0.f, 1.f, 0.f));
  gProj = glm::perspective(glm::radians(50.f), float(gWidth) / float(gHeight), kNear, kFar);
}
}

GLuint makeSSBO(GLuint binding, GLsizeiptr bytes, const void* data) {
  GLuint buffer = 0;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glBufferData(GL_SHADER_STORAGE_BUFFER, bytes, data, GL_DYNAMIC_DRAW);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, buffer);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
  return buffer;
}

void readSSBO(GLuint buffer, GLsizeiptr bytes, void* dst) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, dst);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void clearSSBO(GLuint buffer, uint32_t value) {
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, buffer);
  glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &value);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

GLuint ceilDiv(GLuint n, GLuint d) {
  return d == 0 ? 0 : (n + d - 1) / d;
}

GLuint loadComputeProgram(const char* path) {
  const std::string src = readFile(shaderPath(path));
  return linkProgram(compile(GL_COMPUTE_SHADER, src, path), 0);
}

bool initWindow(int width, int height, const char* title) {
  gWidth = width;
  gHeight = height;
  if (!glfwInit()) {
    return false;
  }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  gWindow = glfwCreateWindow(width, height, title, 0, 0);
  if (!gWindow) {
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(gWindow);
  initGLLoader();
  glfwSwapInterval(1);
  try {
    gGbufferProg = linkProgram(
        compile(GL_VERTEX_SHADER, readFile(shaderPath("gbuffer.vert")), "gbuffer.vert"),
        compile(GL_FRAGMENT_SHADER, readFile(shaderPath("gbuffer.frag")), "gbuffer.frag"));
    gDisplayProg = linkProgram(
        compile(GL_VERTEX_SHADER, readFile(shaderPath("fullscreen.vert")), "fullscreen.vert"),
        compile(GL_FRAGMENT_SHADER, readFile(shaderPath("display.frag")), "display.frag"));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return false;
  }
  uploadCube();
  uploadQuad();
  makeTargets(width, height);
  updateCamera();
  return true;
}

bool windowShouldClose() { return glfwWindowShouldClose(gWindow); }

void pollAndSwap() {
  glfwSwapBuffers(gWindow);
  glfwPollEvents();
  int w = 0;
  int h = 0;
  glfwGetFramebufferSize(gWindow, &w, &h);
  if (w > 0 && h > 0 && (w != gWidth || h != gHeight)) {
    gWidth = w;
    gHeight = h;
    makeTargets(w, h);
    updateCamera();
  }
}

void shutdownWindow() {
  glfwDestroyWindow(gWindow);
  glfwTerminate();
  gWindow = 0;
}

int windowWidth() { return gWidth; }
int windowHeight() { return gHeight; }

void renderGBuffer() {
  updateCamera();
  glBindFramebuffer(GL_FRAMEBUFFER, gFbo);
  glViewport(0, 0, gWidth, gHeight);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(gGbufferProg);
  glUniformMatrix4fv(glGetUniformLocation(gGbufferProg, "uView"), 1, GL_FALSE, &gView[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(gGbufferProg, "uProj"), 1, GL_FALSE, &gProj[0][0]);
  glBindVertexArray(gCube);
  glDrawArrays(GL_TRIANGLES, 0, gCubeCount);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void displayTexture(GLuint texture) {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, gWidth, gHeight);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(gDisplayProg);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture);
  glUniform1i(glGetUniformLocation(gDisplayProg, "tex"), 0);
  glBindVertexArray(gQuad);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

GLuint gbufferPosition() { return gPos; }
GLuint gbufferNormal() { return gNrm; }
GLuint gbufferAlbedo() { return gAlb; }
GLuint gbufferDepth() { return gDepth; }

const glm::mat4& viewMatrix() { return gView; }
const glm::mat4& projMatrix() { return gProj; }
float zNear() { return kNear; }
float zFar() { return kFar; }

float positiveDepthFromWindowZ(float windowZ) {
  const float zndc = windowZ * 2.f - 1.f;
  const float zview = (2.f * kNear * kFar) / (kFar + kNear - zndc * (kFar - kNear));
  return -zview;
}
