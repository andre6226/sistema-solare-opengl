#version 330 core
// One end of a constellation segment, at infinity like the stars (stars.vert).
layout (location = 0) in vec3 aDir;   // J2000 direction, ICRF

uniform mat4 view;
uniform mat4 projection;
uniform mat3 icrfToWorld;

void main() {
    vec4 clip = projection * vec4(mat3(view) * (icrfToWorld * aDir), 1.0);
    gl_Position = vec4(clip.xy, 0.0, clip.w);
}
