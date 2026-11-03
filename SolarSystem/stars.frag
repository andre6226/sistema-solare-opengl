#version 330 core
in vec3 vColor;
in float vPeak;
in float vSigma;
in float vSize;
out vec4 FragColor;

void main() {
    float r = length(gl_PointCoord - 0.5) * vSize;
    FragColor = vec4(vColor * vPeak * exp(-0.5 * r * r / (vSigma * vSigma)), 1.0);
}
