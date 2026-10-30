#version 430 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D tex;
void main() {
  fragColor = texture(tex, vUv);
}
