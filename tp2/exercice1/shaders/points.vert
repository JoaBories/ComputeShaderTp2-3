#version 430 core
layout(location = 0) in vec4 posType;
uniform mat4 uView;
uniform mat4 uProj;
void main() {
  vec4 view = uView * vec4(posType.xyz, 1.0);
  gl_Position = uProj * view;
  gl_PointSize = clamp(400.0 / max(-view.z, 1.0), 1.5, 12.0);
}
