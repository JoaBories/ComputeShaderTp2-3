// z fenetre [0, 1] -> profondeur positive d = -z en repere camera.
uniform float uZNear;
uniform float uZFar;
float positiveDepthFromWindowZ(float windowZ) {
  float zndc = windowZ * 2.0 - 1.0;
  float zview = (2.0 * uZNear * uZFar) / (uZFar + uZNear - zndc * (uZFar - uZNear));
  return -zview;
}
