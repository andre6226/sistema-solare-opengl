#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;

void main() {
    vec4 clipPos = projection * (view * vec4(aPos, 1.0));

    // Exact logarithmic depth formula matching SolarSystem/base.vert (lines 38-41)
    if (clipPos.w > 0.0) {
        const float Fcoef = 2.0 / log2(3.0e10 + 1.0);
        clipPos.z = (log2(max(1e-6, 1.0 + clipPos.w)) * Fcoef - 1.0) * clipPos.w;
    }

    gl_Position = clipPos;
}
