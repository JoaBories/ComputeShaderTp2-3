#version 430 core
layout(location = 0) in vec4 posType;
uniform mat4 uView;
uniform mat4 uProj;
void main() {
  gl_Position = uProj * uView * vec4(posType.xyz, 1.0);
  gl_PointSize = 3.0;
}
