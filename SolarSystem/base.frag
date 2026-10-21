#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

out vec4 FragColor;

// Must stay in sync with enum class BodyType in CelestialBody.hpp
const int BODY_PLANET   = 0;
const int BODY_SUN      = 1;
const int BODY_RING     = 2;
const int BODY_SKY      = 3;
const int BODY_SUN_GLOW = 4;

uniform int bodyType;

uniform vec3 cameraPos;
uniform vec3 lightPos;
uniform sampler2D bodyTexture;

// Rings only: the body casting the shadow onto them.
uniform vec3  parentCenter;
uniform float parentRadius;

// Dynamic Eclipse Shadowing
uniform float sunRadius;
uniform int shadowCasterCount;
uniform vec3 shadowCasterPos[8];
uniform float shadowCasterRadius[8];

// Radial extent of the ring annulus, as a fraction of the quad's half width.
const float RING_INNER = 0.502;   // 70,750 km of the 140,920 km quad, see SolarSystem.hpp
const float RING_OUTER = 1.0;

void main() {
    if (bodyType == BODY_SUN_GLOW) {
        vec3 viewDir = normalize(cameraPos - vWorldPos);
        float ndotv  = clamp(dot(normalize(vNormal), viewDir), 0.0, 1.0);
        float corona = pow(ndotv, 3.2) * 0.55;
        vec3 glowColor = mix(vec3(1.0, 0.55, 0.12), vec3(1.0, 0.96, 0.85), pow(ndotv, 1.6));
        FragColor = vec4(glowColor * corona, corona);
        return;
    }

    bool isRing = (bodyType == BODY_RING);

    vec2 uv = vTexCoord;
    bool outsideAnnulus = false;

    if (isRing) {
        // Carve an annulus out of the quad and turn the radial distance into a
        // 1D lookup along the ring texture.
        float dist = length(vTexCoord - vec2(0.5, 0.5)) * 2.0;
        outsideAnnulus = (dist < RING_INNER || dist > RING_OUTER);
        uv = vec2(clamp((dist - RING_INNER) / (RING_OUTER - RING_INNER), 0.0, 1.0), 0.5);
    }

    // Sampled before any discard: texture() picks its mip level from the
    // derivatives of uv across neighbouring fragments, and those are only
    // well defined while all of them are still alive.
    vec4 texColor = texture(bodyTexture, uv);

    // The alpha test only concerns the rings.
    if (isRing && (outsideAnnulus || texColor.a < 0.1 || length(texColor.rgb) < 0.15)) {
        discard;
    }

    if (bodyType == BODY_SKY) {
        FragColor = texColor;
        return;
    }

    if (bodyType == BODY_SUN) {
        vec3 viewDir = normalize(cameraPos - vWorldPos);
        float ndotv  = clamp(dot(normalize(vNormal), viewDir), 0.0, 1.0);
        // Overexpose the solar texture so granulation shows through a blinding
        // white-gold incandescence up close.
        vec3 texGlow = pow(texColor.rgb, vec3(0.80)) * 1.9;
        vec3 dazzle  = vec3(1.0, 0.94, 0.78) * (0.45 + 0.55 * pow(ndotv, 1.8));
        FragColor = vec4(texGlow + dazzle, 1.0);
        return;
    }

    vec3 normal   = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vWorldPos);
    vec3 viewDir  = normalize(cameraPos - vWorldPos);

    vec3 albedo = texColor.rgb;

    // Lambert (diffuse)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Blinn-Phong (specular), gated by the same cosine as the diffuse term.
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3) * diff;

    vec3 ambient = vec3(0.008);

    // Dynamic Eclipse Shadowing (Solar & Lunar)
    float shadowFactor = 1.0;
    if (diff > 0.0) {
        for (int i = 0; i < shadowCasterCount; ++i) {
            vec3 casterPos = shadowCasterPos[i];
            float casterRadius = shadowCasterRadius[i];
            vec3 a = normalize(casterPos - lightPos);
            float dSC = length(casterPos - lightPos);
            vec3 v = vWorldPos - casterPos;
            float z = dot(v, a);
            if (z > 0.0) {
                float tanUmbra = (sunRadius - casterRadius) / dSC;
                float tanPenumbra = (sunRadius + casterRadius) / dSC;
                float rUmbra = casterRadius - z * tanUmbra;
                float rPenumbra = casterRadius + z * tanPenumbra;
                vec3 vPerp = v - z * a;
                float dPerpSq = dot(vPerp, vPerp);
                if (dPerpSq < rPenumbra * rPenumbra) {
                    float dPerp = sqrt(dPerpSq);
                    float umbraBound = max(0.0, rUmbra);
                    float s_i = smoothstep(umbraBound, rPenumbra, dPerp);
                    if (rUmbra < 0.0) { // Annular eclipse
                        float occultation = clamp((casterRadius * dSC) / (sunRadius * z), 0.0, 1.0);
                        float sMin = 1.0 - (occultation * occultation);
                        s_i = mix(sMin, 1.0, s_i);
                    }
                    shadowFactor *= s_i;
                }
            }
        }
    }
    diffuse *= shadowFactor;
    specular *= shadowFactor;

    if (isRing) {
        vec3 rayDir = normalize(vWorldPos - lightPos);
        float projectedDistance = dot(parentCenter - lightPos, rayDir);
        vec3 closestPoint = lightPos + rayDir * projectedDistance;
        float distanceFromAxis = length(closestPoint - parentCenter);

        bool behindPlanet = projectedDistance < length(vWorldPos - lightPos);

        if (distanceFromAxis < (parentRadius * 0.98) && behindPlanet) {
            ambient  = vec3(0.005);
            diffuse  = vec3(0.0);
            specular = vec3(0.0);
            albedo  *= 0.15;
        } else {
            ambient = vec3(0.8);
        }
    }

    vec3 color = (ambient + diffuse) * albedo + specular;

    FragColor = vec4(color, texColor.a);
}
