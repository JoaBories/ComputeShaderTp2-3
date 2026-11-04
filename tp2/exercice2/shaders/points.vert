#version 430 core
layout(location = 0) in vec4 posType;

uniform mat4 uView;

out VS_OUT {
  vec3 viewPos;
  float type;
} vs_out;

void main() {
  vs_out.viewPos = (uView * vec4(posType.xyz, 1.0)).xyz;
  vs_out.type = posType.w;
}
