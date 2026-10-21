#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;
uniform int  bodyType;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vTexCoord;

const int BODY_SKY = 3;

void main() {
    vTexCoord = aTexCoords;

    // The sky sphere is drawn first at infinity with depth writes off: only its
    // view-space direction matters, so it never clips against nearPlane or farPlane.
    if (bodyType == BODY_SKY) {
        vec3 dir = mat3(view) * normalize(mat3(model) * aPos);
        vec4 clip = projection * vec4(dir, 1.0);
        gl_Position = vec4(clip.xy, 0.0, clip.w);
        return;
    }

    vec4 worldPos = model * vec4(aPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize(normalMatrix * aNormal);

    vec4 clipPos = projection * (view * worldPos);

    // Logarithmic depth keeps 24-bit depth resolution uniform from a 5 km
    // asteroid up to 3e10 km, preventing far-plane depth collapse to 1.0.
    if (clipPos.w > 0.0) {
        const float Fcoef = 2.0 / log2(3.0e10 + 1.0);
        clipPos.z = (log2(max(1e-6, 1.0 + clipPos.w)) * Fcoef - 1.0) * clipPos.w;
    }

    gl_Position = clipPos;
}
