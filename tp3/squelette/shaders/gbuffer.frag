#version 430 core
in vec3 vWorld;
in vec3 vNrm;
uniform mat4 uView;
layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outNrm;
layout(location = 2) out vec4 outAlb;
void main() {
  vec4 viewPos = uView * vec4(vWorld, 1.0);
  outPos = vec4(viewPos.xyz, 1.0);
  vec3 n = mat3(uView) * normalize(vNrm);
  outNrm = vec4(normalize(n), 0.0);
  outAlb = vec4(0.75, 0.75, 0.75, 1.0);
}
