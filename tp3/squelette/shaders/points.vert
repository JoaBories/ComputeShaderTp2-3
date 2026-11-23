#version 430 core
layout(location = 0) in vec4 posRad;
layout(location = 1) in vec4 color;

uniform mat4 uView;

out VS_OUT {
  vec3 viewPos;
  vec4 color;
  flat int id;
} vs_out;

void main() {
  vs_out.viewPos = (uView * vec4(posRad.xyz, 1.0)).xyz;
  vs_out.color = color;
  vs_out.id = gl_VertexID;
}
