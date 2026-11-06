#include "gl_utils.hpp"

#include "vbsp.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <GLFW/glfw3.h>
#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
GLuint gPreviewProg = 0;
glm::vec3 gEye(32.f, 8.f, 42.f);
float gYaw = 0.f;
float gPitch = -0.15f;
bool gLook = false;
double gLastMouseX = 0.0;
double gLastMouseY = 0.0;
bool gHaveMouse = false;
GLuint gQuad = 0;
struct MapPart {
  GLuint vao;
  GLuint tex;
  GLsizei count;
  float color[3];
  int textured;
};
std::vector<MapPart> gParts;

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

glm::vec3 mapPos(const aiVector3D& p) {
  return glm::vec3(p.x, p.z, -p.y);
}

void appendMesh(const aiScene* scene, const aiMesh* mesh, std::vector<float>& xyz, std::vector<float>& nrm, std::vector<float>& uv) {
  (void)scene;
  for (unsigned f = 0; f < mesh->mNumFaces; ++f) {
    const aiFace& face = mesh->mFaces[f];
    if (face.mNumIndices != 3) continue;
    for (unsigned k = 0; k < 3; ++k) {
      const unsigned id = face.mIndices[k];
      const glm::vec3 p = mapPos(mesh->mVertices[id]);
      xyz.push_back(p.x); xyz.push_back(p.y); xyz.push_back(p.z);
      aiVector3D n(0.f, 1.f, 0.f);
      if (mesh->mNormals) n = mesh->mNormals[id];
      const glm::vec3 nn = glm::normalize(mapPos(n));
      nrm.push_back(nn.x); nrm.push_back(nn.y); nrm.push_back(nn.z);
      if (mesh->mTextureCoords[0]) {
        uv.push_back(mesh->mTextureCoords[0][id].x);
        uv.push_back(mesh->mTextureCoords[0][id].y);
      } else {
        uv.push_back(0.f); uv.push_back(0.f);
      }
    }
  }
}

GLuint textureOf(const aiScene* scene, unsigned material) {
  if (material >= scene->mNumMaterials) return 0;
  aiString name;
  if (scene->mMaterials[material]->GetTexture(aiTextureType_DIFFUSE, 0, &name) != AI_SUCCESS) return 0;
  const aiTexture* tex = scene->GetEmbeddedTexture(name.C_Str());
  if (!tex || tex->mHeight == 0) return 0;
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(tex->mWidth), static_cast<GLsizei>(tex->mHeight), 0, GL_RGBA, GL_UNSIGNED_BYTE, tex->pcData);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glBindTexture(GL_TEXTURE_2D, 0);
  return id;
}

void uploadRaw(const std::vector<float>& xyz, const std::vector<float>& nrm) {
  glm::vec3 mn(1e30f);
  glm::vec3 mx(-1e30f);
  for (size_t v = 0; v + 2 < xyz.size(); v += 3) {
    mn = glm::min(mn, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
    mx = glm::max(mx, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
  }
  const float extent = std::max(mx.x - mn.x, std::max(mx.y - mn.y, mx.z - mn.z));
  const float scale = 56.f / (extent > 1e-6f ? extent : 1.f);
  const glm::vec3 center = (mn + mx) * 0.5f;
  std::vector<float> packed(xyz.size() / 3u * 8u);
  for (size_t v = 0, o = 0; v + 2 < xyz.size(); v += 3, o += 8) {
    packed[o] = (xyz[v] - center.x) * scale + 32.f;
    packed[o + 1] = (xyz[v + 1] - mn.y) * scale + 4.f;
    packed[o + 2] = (xyz[v + 2] - center.z) * scale + 32.f;
    packed[o + 3] = nrm[v];
    packed[o + 4] = nrm[v + 1];
    packed[o + 5] = nrm[v + 2];
    packed[o + 6] = 0.f;
    packed[o + 7] = 0.f;
  }
  MapPart part;
  part.count = static_cast<GLsizei>(xyz.size() / 3u);
  part.tex = 0;
  part.textured = 0;
  part.color[0] = 0.62f;
  part.color[1] = 0.55f;
  part.color[2] = 0.42f;
  glGenVertexArrays(1, &part.vao);
  GLuint buf = 0;
  glGenBuffers(1, &buf);
  glBindVertexArray(part.vao);
  glBindBuffer(GL_ARRAY_BUFFER, buf);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(packed.size() * sizeof(float)), packed.data(), GL_STATIC_DRAW);
  const GLsizei stride = 8 * sizeof(float);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
  glBindVertexArray(0);
  gParts.clear();
  gParts.push_back(part);
}

void uploadSurfaces(const std::vector<LevelSurface>& surfs) {
  glm::vec3 mn(1e30f);
  glm::vec3 mx(-1e30f);
  for (size_t s = 0; s < surfs.size(); ++s) {
    const std::vector<float>& xyz = surfs[s].positions;
    for (size_t v = 0; v + 2 < xyz.size(); v += 3) {
      mn = glm::min(mn, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
      mx = glm::max(mx, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
    }
  }
  const float extent = std::max(mx.x - mn.x, std::max(mx.y - mn.y, mx.z - mn.z));
  const float scale = 56.f / (extent > 1e-6f ? extent : 1.f);
  const glm::vec3 center = (mn + mx) * 0.5f;
  gParts.clear();
  for (size_t s = 0; s < surfs.size(); ++s) {
    const LevelSurface& src = surfs[s];
    if (src.positions.empty()) continue;
    std::vector<float> packed(src.positions.size() / 3u * 8u);
    for (size_t v = 0, o = 0; v + 2 < src.positions.size(); v += 3, o += 8) {
      packed[o] = (src.positions[v] - center.x) * scale + 32.f;
      packed[o + 1] = (src.positions[v + 1] - mn.y) * scale + 4.f;
      packed[o + 2] = (src.positions[v + 2] - center.z) * scale + 32.f;
      packed[o + 3] = src.normals[v];
      packed[o + 4] = src.normals[v + 1];
      packed[o + 5] = src.normals[v + 2];
      packed[o + 6] = src.uvs[v / 3 * 2];
      packed[o + 7] = src.uvs[v / 3 * 2 + 1];
    }
    MapPart part;
    part.count = static_cast<GLsizei>(src.positions.size() / 3u);
    part.tex = 0;
    part.textured = 0;
    part.color[0] = 0.62f;
    part.color[1] = 0.55f;
    part.color[2] = 0.42f;
    if (!src.rgba.empty() && src.width > 0 && src.height > 0) {
      glGenTextures(1, &part.tex);
      glBindTexture(GL_TEXTURE_2D, part.tex);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, src.width, src.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, src.rgba.data());
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glBindTexture(GL_TEXTURE_2D, 0);
      part.textured = 1;
    }
    glGenVertexArrays(1, &part.vao);
    GLuint buf = 0;
    glGenBuffers(1, &buf);
    glBindVertexArray(part.vao);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(packed.size() * sizeof(float)), packed.data(), GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    gParts.push_back(part);
  }
  if (gParts.empty()) throw std::runtime_error("aucun triangle");
}

void buildMap(const char* path) {
  {
    std::ifstream probe(path, std::ios::binary);
    char mag[4] = {};
    probe.read(mag, 4);
    if (std::memcmp(mag, "VBSP", 4) == 0) {
      std::vector<float> xyz;
      std::vector<float> nrm;
      std::string err;
      if (!readSourceBsp(path, xyz, nrm, err)) {
        throw std::runtime_error(err + " (" + path + ")");
      }
      uploadRaw(xyz, nrm);
      return;
    }
    if (std::memcmp(mag, "PK\x03\x04", 4) == 0) {
      std::vector<unsigned char> bsp;
      std::string err;
      if (!extractBspFromPk3(path, bsp, err)) {
        throw std::runtime_error(err + " (" + path + ")");
      }
      if (bsp.size() >= 4 && std::memcmp(bsp.data(), "VBSP", 4) == 0) {
        std::vector<float> xyz;
        std::vector<float> nrm;
        if (!readSourceBspBuffer(bsp, xyz, nrm, err)) {
          throw std::runtime_error(err + " (" + path + ")");
        }
        uploadRaw(xyz, nrm);
        return;
      }
      if (bsp.size() >= 4 && std::memcmp(bsp.data(), "IBSP", 4) == 0) {
        std::vector<LevelSurface> surfs;
        if (!readQuakePk3(path, bsp, surfs, err)) {
          throw std::runtime_error(err + " (" + path + ")");
        }
        uploadSurfaces(surfs);
        return;
      }
      throw std::runtime_error(std::string("bsp inconnu dans ") + path);
    }
  }
  Assimp::Importer importer;
  const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices | aiProcess_PreTransformVertices);
  if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || scene->mNumMeshes == 0) {
    const char* err = importer.GetErrorString();
    throw std::runtime_error(std::string("Assimp : ") + (err && err[0] ? err : "scene vide") + " (" + path + ")");
  }
  std::vector<std::vector<float> > chunks(scene->mNumMeshes * 3u);
  glm::vec3 mn(1e30f);
  glm::vec3 mx(-1e30f);
  for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
    appendMesh(scene, scene->mMeshes[i], chunks[i * 3], chunks[i * 3 + 1], chunks[i * 3 + 2]);
    const std::vector<float>& xyz = chunks[i * 3];
    for (size_t v = 0; v + 2 < xyz.size(); v += 3) {
      mn = glm::min(mn, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
      mx = glm::max(mx, glm::vec3(xyz[v], xyz[v + 1], xyz[v + 2]));
    }
  }
  const float extent = std::max(mx.x - mn.x, std::max(mx.y - mn.y, mx.z - mn.z));
  const float scale = 56.f / extent;
  const glm::vec3 center = (mn + mx) * 0.5f;
  gParts.clear();
  for (unsigned i = 0; i < scene->mNumMeshes; ++i) {
    const std::vector<float>& xyz = chunks[i * 3];
    const std::vector<float>& nrm = chunks[i * 3 + 1];
    const std::vector<float>& uv = chunks[i * 3 + 2];
    if (xyz.empty()) continue;
    std::vector<float> packed(xyz.size() / 3u * 8u);
    for (size_t v = 0, o = 0; v + 2 < xyz.size(); v += 3, o += 8) {
      packed[o] = (xyz[v] - center.x) * scale + 32.f;
      packed[o + 1] = (xyz[v + 1] - mn.y) * scale + 4.f;
      packed[o + 2] = (xyz[v + 2] - center.z) * scale + 32.f;
      packed[o + 3] = nrm[v];
      packed[o + 4] = nrm[v + 1];
      packed[o + 5] = nrm[v + 2];
      packed[o + 6] = uv[(v / 3) * 2];
      packed[o + 7] = uv[(v / 3) * 2 + 1];
    }
    MapPart part;
    part.count = static_cast<GLsizei>(xyz.size() / 3u);
    part.tex = textureOf(scene, scene->mMeshes[i]->mMaterialIndex);
    part.textured = part.tex != 0;
    part.color[0] = part.color[1] = part.color[2] = 0.7f;
    if (scene->mMeshes[i]->mMaterialIndex < scene->mNumMaterials) {
      aiColor3D kd(0.7f, 0.7f, 0.7f);
      scene->mMaterials[scene->mMeshes[i]->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, kd);
      part.color[0] = kd.r; part.color[1] = kd.g; part.color[2] = kd.b;
    }
    glGenVertexArrays(1, &part.vao);
    GLuint buf = 0;
    glGenBuffers(1, &buf);
    glBindVertexArray(part.vao);
    glBindBuffer(GL_ARRAY_BUFFER, buf);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(packed.size() * sizeof(float)), packed.data(), GL_STATIC_DRAW);
    const GLsizei stride = 8 * sizeof(float);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glBindVertexArray(0);
    gParts.push_back(part);
  }
  if (gParts.empty()) throw std::runtime_error(std::string("aucun triangle dans ") + path);
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

void onKey(GLFWwindow*, int key, int, int action, int) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    gLook = !gLook;
    glfwSetInputMode(gWindow, GLFW_CURSOR, gLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    gHaveMouse = false;
  }
}

void onMouseButton(GLFWwindow*, int button, int action, int) {
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !gLook) {
    gLook = true;
    glfwSetInputMode(gWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    gHaveMouse = false;
  }
}

void updateCamera() {
  const float now = static_cast<float>(glfwGetTime());
  static float prev = now;
  float dt = now - prev;
  prev = now;
  if (dt < 0.f || dt > 0.1f) {
    dt = 0.f;
  }
  if (gLook) {
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(gWindow, &x, &y);
    if (gHaveMouse) {
      gYaw += static_cast<float>(x - gLastMouseX) * 0.0022f;
      gPitch += static_cast<float>(gLastMouseY - y) * 0.0022f;
      if (gPitch > 1.5f) {
        gPitch = 1.5f;
      }
      if (gPitch < -1.5f) {
        gPitch = -1.5f;
      }
    }
    gLastMouseX = x;
    gLastMouseY = y;
    gHaveMouse = true;
  }
  const glm::vec3 forward(std::sin(gYaw), 0.f, -std::cos(gYaw));
  const glm::vec3 right(std::cos(gYaw), 0.f, std::sin(gYaw));
  glm::vec3 move(0.f);
  if (glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS) {
    move += forward;
  }
  if (glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS) {
    move -= forward;
  }
  if (glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS) {
    move += right;
  }
  if (glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS) {
    move -= right;
  }
  if (glfwGetKey(gWindow, GLFW_KEY_SPACE) == GLFW_PRESS) {
    move.y += 1.f;
  }
  if (glfwGetKey(gWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(gWindow, GLFW_KEY_C) == GLFW_PRESS) {
    move.y -= 1.f;
  }
  const float speed = glfwGetKey(gWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 28.f : 10.f;
  if (glm::dot(move, move) > 0.f) {
    gEye += glm::normalize(move) * speed * dt;
  }
  const glm::vec3 look(std::cos(gPitch) * std::sin(gYaw), std::sin(gPitch), -std::cos(gPitch) * std::cos(gYaw));
  gView = glm::lookAt(gEye, gEye + look, glm::vec3(0.f, 1.f, 0.f));
  gProj = glm::perspective(glm::radians(70.f), float(gWidth) / float(gHeight), kNear, kFar);
}
}

bool loadMap(const char* path) {
  try {
    buildMap(path);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return false;
  }
  return true;
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
  glfwSetKeyCallback(gWindow, onKey);
  glfwSetMouseButtonCallback(gWindow, onMouseButton);
  initGLLoader();
  glfwSwapInterval(1);
  try {
    gGbufferProg = linkProgram(
        compile(GL_VERTEX_SHADER, readFile(shaderPath("gbuffer.vert")), "gbuffer.vert"),
        compile(GL_FRAGMENT_SHADER, readFile(shaderPath("gbuffer.frag")), "gbuffer.frag"));
    gDisplayProg = linkProgram(
        compile(GL_VERTEX_SHADER, readFile(shaderPath("fullscreen.vert")), "fullscreen.vert"),
        compile(GL_FRAGMENT_SHADER, readFile(shaderPath("display.frag")), "display.frag"));
    gPreviewProg = linkProgram(
        compile(GL_VERTEX_SHADER, readFile(shaderPath("fullscreen.vert")), "fullscreen.vert"),
        compile(GL_FRAGMENT_SHADER, readFile(shaderPath("preview.frag")), "preview.frag"));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "%s\n", e.what());
    return false;
  }
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
  glUniform1i(glGetUniformLocation(gGbufferProg, "uMap"), 0);
  for (size_t i = 0; i < gParts.size(); ++i) {
    const MapPart& part = gParts[i];
    glUniform1i(glGetUniformLocation(gGbufferProg, "uHasMap"), part.textured);
    glUniform3fv(glGetUniformLocation(gGbufferProg, "uColor"), 1, part.color);
    glActiveTexture(GL_TEXTURE0);
    if (part.textured) glBindTexture(GL_TEXTURE_2D, part.tex);
    glBindVertexArray(part.vao);
    glDrawArrays(GL_TRIANGLES, 0, part.count);
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void displayPreview() {
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, gWidth, gHeight);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(gPreviewProg);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, gPos);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, gNrm);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, gAlb);
  glUniform1i(glGetUniformLocation(gPreviewProg, "posTex"), 0);
  glUniform1i(glGetUniformLocation(gPreviewProg, "nrmTex"), 1);
  glUniform1i(glGetUniformLocation(gPreviewProg, "albTex"), 2);
  glBindVertexArray(gQuad);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glActiveTexture(GL_TEXTURE0);
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
