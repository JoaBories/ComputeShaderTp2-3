#version 430 core
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D posTex;
uniform sampler2D nrmTex;
uniform sampler2D albTex;
void main() {
  vec3 n = texture(nrmTex, vUv).xyz;
  vec3 alb = texture(albTex, vUv).rgb;
  if (dot(n, n) < 1e-4) {
    fragColor = vec4(0.07, 0.08, 0.10, 1.0);
    return;
  }
  vec3 L = normalize(vec3(0.25, 0.55, 1.0));
  float ndl = clamp(dot(normalize(n), L), 0.0, 1.0);
  fragColor = vec4(alb * (0.28 + 0.72 * ndl), 1.0);
}
