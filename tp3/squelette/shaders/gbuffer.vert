#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec2 aUv;
uniform mat4 uView;
uniform mat4 uProj;
out vec3 vWorld;
out vec3 vNrm;
out vec2 vUv;
void main() {
  vWorld = aPos;
  vNrm = aNrm;
  vUv = aUv;
  gl_Position = uProj * uView * vec4(aPos, 1.0);
}
