#version 330 core
// One main-belt asteroid per vertex, moved along its Kepler ellipse on the GPU.
layout (location = 0) in vec4 aElements;   // a (AU), e, inclination, node (rad)
layout (location = 1) in vec3 aPhase;      // periapsis, mean anomaly at epoch (rad), mean motion (rad/day)

uniform mat4 view;
uniform mat4 projection;
uniform vec3 sunPos;          // render space, km
uniform float daysSinceEpoch;
uniform mat3 eclipticToWorld;

out float vFade;

void main() {
    float a = aElements.x, e = aElements.y, inc = aElements.z, node = aElements.w, w = aPhase.x;
    float M = mod(aPhase.y + aPhase.z * daysSinceEpoch, 6.2831853);
    float E = M + e * sin(M);
    for (int k = 0; k < 5; ++k) E -= (E - e * sin(E) - M) / (1.0 - e * cos(E));
    float x = a * (cos(E) - e), y = a * sqrt(1.0 - e * e) * sin(E);
    float cO = cos(node), sO = sin(node), cw = cos(w), sw = sin(w), ci = cos(inc), si = sin(inc);
    vec3 P = vec3(cO * cw - sO * sw * ci, sO * cw + cO * sw * ci, sw * si);
    vec3 Q = vec3(-cO * sw - sO * cw * ci, -sO * sw + cO * cw * ci, cw * si);
    vec3 worldPos = sunPos + eclipticToWorld * ((P * x + Q * y) * 1.495978707e8);

    vec4 viewPos = view * vec4(worldPos, 1.0);
    // Up close a single dot would read as a nearby rock: fade those within ~2 million km.
    vFade = clamp(-viewPos.z / 2.0e6 - 0.5, 0.0, 1.0);
    vec4 clipPos = projection * viewPos;
    if (clipPos.w > 0.0) {
        const float Fcoef = 2.0 / log2(3.0e10 + 1.0);
        clipPos.z = (log2(max(1e-6, 1.0 + clipPos.w)) * Fcoef - 1.0) * clipPos.w;
    }
    gl_Position = clipPos;
}
