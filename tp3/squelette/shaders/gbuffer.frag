#version 430 core
in vec3 vWorld;
in vec3 vNrm;
in vec2 vUv;
uniform mat4 uView;
uniform sampler2D uMap;
uniform vec3 uColor;
uniform int uHasMap;
layout(location = 0) out vec4 outPos;
layout(location = 1) out vec4 outNrm;
layout(location = 2) out vec4 outAlb;
void main() {
  vec4 viewPos = uView * vec4(vWorld, 1.0);
  outPos = vec4(viewPos.xyz, 1.0);
  vec3 n = mat3(uView) * normalize(vNrm);
  outNrm = vec4(normalize(n), 0.0);
  vec3 alb = uHasMap != 0 ? texture(uMap, vUv).rgb : uColor;
  alb *= 0.55 + 0.45 * clamp(normalize(vNrm).y * 0.5 + 0.5, 0.0, 1.0);
  outAlb = vec4(alb, 1.0);
}
