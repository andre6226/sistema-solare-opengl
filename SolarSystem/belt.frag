#version 330 core
in float vFade;
uniform float beltAlpha;
out vec4 FragColor;

void main() {
    FragColor = vec4(0.62, 0.56, 0.48, 0.45 * vFade * beltAlpha);
}
