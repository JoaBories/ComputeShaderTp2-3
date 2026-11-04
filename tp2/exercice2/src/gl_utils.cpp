#include "gl_utils.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
GLFWwindow* gWindow = 0;
int gWidth = 800;
int gHeight = 800;
glm::mat4 gView(1.f);
glm::mat4 gProj(1.f);
GLuint gPoints = 0;
const glm::vec3 gTarget(32.f, 32.f, 32.f);
float gYaw = 0.f;
float gPitch = 0.f;
float gDist = 88.f;
bool gDragging = false;
double gLastX = 0.0;
double gLastY = 0.0;

void updateView() {
  const float cy = std::cos(gYaw), sy = std::sin(gYaw);
  const float cp = std::cos(gPitch), sp = std::sin(gPitch);
  const glm::vec3 eye = gTarget + glm::vec3(gDist * cp * sy, gDist * sp, gDist * cp * cy);
  gView = glm::lookAt(eye, gTarget, glm::vec3(0.f, 1.f, 0.f));
}
void onMouseButton(GLFWwindow*, int button, int action, int) {
  if (button != GLFW_MOUSE_BUTTON_LEFT) return;
  gDragging = action == GLFW_PRESS;
  glfwGetCursorPos(gWindow, &gLastX, &gLastY);
}
void onCursor(GLFWwindow*, double x, double y) {
  if (!gDragging) return;
  const float dx = static_cast<float>(x - gLastX);
  const float dy = static_cast<float>(y - gLastY);
  gLastX = x;
  gLastY = y;
  gYaw -= dx * 0.005f;
  gPitch += dy * 0.005f;
  if (gPitch > 1.5f) gPitch = 1.5f;
  if (gPitch < -1.5f) gPitch = -1.5f;
  updateView();
}
void onScroll(GLFWwindow*, double, double yoff) {
  gDist *= yoff > 0.0 ? 0.9f : 1.1f;
  if (gDist < 10.f) gDist = 10.f;
  if (gDist > 300.f) gDist = 300.f;
  updateView();
}

std::string readFile(const std::string& path) {
  std::ifstream in(path.c_str());
  if (!in) throw std::runtime_error("lecture impossible : " + path);
  std::stringstream ss; ss << in.rdbuf(); return ss.str();
}
GLuint compile(GLenum type, const std::string& src, const std::string& name) {
  GLuint shader = glCreateShader(type);
  const char* c = src.c_str();
  glShaderSource(shader, 1, &c, 0);
  glCompileShader(shader);
  GLint ok = 0; glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048]; glGetShaderInfoLog(shader, sizeof(log), 0, log);
    throw std::runtime_error(name + " : " + log);
  }
  return shader;
}
GLuint link(GLuint a, GLuint b) {
  GLuint program = glCreateProgram();
  glAttachShader(program, a);
  if (b) glAttachShader(program, b);
  glLinkProgram(program);
  GLint ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048]; glGetProgramInfoLog(program, sizeof(log), 0, log);
    throw std::runtime_error(std::string("link : ") + log);
  }
  glDeleteShader(a); if (b) glDeleteShader(b);
  return program;
}
std::string shaderPath(const char* path) {
  std::ifstream direct(path);
  if (direct) return path;
  return std::string(SHADER_DIR) + "/" + path;
}
void ensurePoints() {
  if (gPoints) return;
  gPoints = link(compile(GL_VERTEX_SHADER, readFile(shaderPath("points.vert")), "points.vert"),
                 compile(GL_FRAGMENT_SHADER, readFile(shaderPath("points.frag")), "points.frag"));
  updateView();
  gProj = glm::perspective(glm::radians(45.f), 1.f, 0.1f, 400.f);
}
}

GLuint makeSSBO(GLuint binding, GLsizeiptr bytes, const void* data) {
  GLuint buffer = 0; glGenBuffers(1, &buffer);
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
GLuint ceilDiv(GLuint n, GLuint d) { return d == 0 ? 0 : (n + d - 1) / d; }
GLuint loadComputeProgram(const char* path) {
  return link(compile(GL_COMPUTE_SHADER, readFile(shaderPath(path)), path), 0);
}
bool initWindow(int width, int height, const char* title) {
  gWidth = width; gHeight = height;
  if (!glfwInit()) return false;
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  gWindow = glfwCreateWindow(width, height, title, 0, 0);
  if (!gWindow) { glfwTerminate(); return false; }
  glfwMakeContextCurrent(gWindow);
  glfwSetMouseButtonCallback(gWindow, onMouseButton);
  glfwSetCursorPosCallback(gWindow, onCursor);
  glfwSetScrollCallback(gWindow, onScroll);
  initGLLoader();
  glfwSwapInterval(1);
  return true;
}
bool windowShouldClose() { return glfwWindowShouldClose(gWindow); }
void pollAndSwap() {
  glfwSwapBuffers(gWindow); glfwPollEvents();
  int w = 0, h = 0; glfwGetFramebufferSize(gWindow, &w, &h);
  if (w > 0 && h > 0) { gWidth = w; gHeight = h; }
}
void shutdownWindow() { glfwDestroyWindow(gWindow); glfwTerminate(); gWindow = 0; }
int windowWidth() { return gWidth; }
int windowHeight() { return gHeight; }
const glm::mat4& viewMatrix() { return gView; }
const glm::mat4& projMatrix() { return gProj; }
void drawPoints(GLuint particleBuffer, GLsizei count) {
  ensurePoints();
  glViewport(0, 0, windowWidth(), windowHeight());
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_PROGRAM_POINT_SIZE);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(gPoints);
  glUniformMatrix4fv(glGetUniformLocation(gPoints, "uView"), 1, GL_FALSE, &gView[0][0]);
  glUniformMatrix4fv(glGetUniformLocation(gPoints, "uProj"), 1, GL_FALSE, &gProj[0][0]);
  GLuint vao = 0; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, particleBuffer);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glDrawArrays(GL_POINTS, 0, count);
  glDeleteVertexArrays(1, &vao);
}
