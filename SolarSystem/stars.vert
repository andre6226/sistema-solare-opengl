#version 330 core
// One catalogue star per vertex, at infinity (only its direction matters).
layout (location = 0) in vec3 aDir;     // J2000 direction, ICRF
layout (location = 1) in vec3 aPm;      // proper motion, rad/yr
layout (location = 2) in float aMag;    // V
layout (location = 3) in vec3 aColor;   // linear sRGB, brightest channel 1

uniform mat4 view;
uniform mat4 projection;
uniform mat3 icrfToWorld;
uniform float yearsSinceJ2000;

out vec3 vColor;
out float vPeak;
out float vSigma;
out float vSize;

// Stars brighter than this fill a pixel: their light then spreads over a wider spot,
// as in a photo, keeping the light on screen proportional to the flux, 10^(-0.4 V).
const float SATURATION_MAG = 3.0;
const float SIGMA_PX = 0.7;   // a faint star's spot, Gaussian sigma in pixels

void main() {
    vec3 dir = icrfToWorld * normalize(aDir + aPm * yearsSinceJ2000);
    vec4 clip = projection * vec4(mat3(view) * dir, 1.0);
    gl_Position = vec4(clip.xy, 0.0, clip.w);
    float flux = pow(10.0, -0.4 * (aMag - SATURATION_MAG));
    vPeak = min(flux, 1.0);
    vSigma = SIGMA_PX * sqrt(max(flux, 1.0));
    vSize = 2.0 * ceil(3.0 * vSigma) + 1.0;
    gl_PointSize = vSize;
    vColor = aColor;
}
