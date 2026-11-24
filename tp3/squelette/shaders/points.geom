#version 430 core

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

uniform mat4 uProj;

in VS_OUT {
  vec3 viewPos;
  vec4 color;
  flat int id;
} geomIn[];


layout(std430, binding = 1) buffer Vis {
  uint visibleCount; // compte les lumieres retenues
  uint visible[]; // indices des lumieres retenues
};

out vec4 color;

void main() {
  bool isVisible = false;
  
  if (visibleCount <= 0) return;
  for (int i = 0; i < visibleCount; i++)
  {
    if (visible[i] == geomIn[0].id) 
    { 
      isVisible = true;
      break;
    }
  }
  
  vec3 center = geomIn[0].viewPos;
  
  const vec2 offsets[3] = {
    vec2( 0.00, -1.0),
    vec2(-0.86,  0.5),
    vec2( 0.86,  0.5)
  };

  for (int i = 0; i < 3; ++i) {
    vec3 pos = center + vec3(offsets[i] * 0.35, 0.0);
    gl_Position = uProj * vec4(pos, 1.0);
    color = isVisible ? geomIn[0].color : vec4(1.0f,1.0f,1.0f,1.0f);
    EmitVertex();
  }
  EndPrimitive();
}
