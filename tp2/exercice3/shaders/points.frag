#version 430 core
out vec4 fragColor;
in float type;

void main() {
  fragColor = mix(vec4(1.0, 0.1, 0.1, 1.0), vec4(0.1, 0.2, 1.0, 1.0), (type + 1.0) / 2.0);
}
