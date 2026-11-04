#version 430 core

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 uProj;

in VS_OUT {
  vec3 viewPos;
  float type;
} geomIn[];

out float type;

void main() {
  vec3 center = geomIn[0].viewPos;
  
  const vec2 offsets[3] = {
    vec2( 0.00, -1.0 * geomIn[0].type),
    vec2(-0.86,  0.5 * geomIn[0].type),
    vec2( 0.86,  0.5 * geomIn[0].type)
  };

  for (int i = 0; i < 3; ++i) {
    vec3 pos = center + vec3(offsets[i] * 0.35, 0.0);
    gl_Position = uProj * vec4(pos, 1.0);
    type = geomIn[0].type;
    EmitVertex();
  }
  EndPrimitive();
}
