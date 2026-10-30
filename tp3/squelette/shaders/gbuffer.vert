#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorld;
out vec3 vNrm;
void main() {
  vWorld = aPos;
  vNrm = aNrm;
  gl_Position = uProj * uView * vec4(aPos, 1.0);
}
