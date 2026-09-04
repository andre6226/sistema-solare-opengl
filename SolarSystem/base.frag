#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vTexCoord;

out vec4 FragColor;

// Must stay in sync with enum class BodyType in CelestialBody.hpp
const int BODY_PLANET = 0;
const int BODY_SUN    = 1;
const int BODY_RING   = 2;
const int BODY_SKY    = 3;

uniform int bodyType;

uniform vec3 cameraPos;
uniform vec3 lightPos;
uniform sampler2D bodyTexture;

// Rings only: the body casting the shadow onto them.
uniform vec3  parentCenter;
uniform float parentRadius;

// Radial extent of the ring annulus, as a fraction of the quad's half width.
const float RING_INNER = 0.50;
const float RING_OUTER = 0.95;

void main() {
    bool isRing = (bodyType == BODY_RING);

    vec2 uv = vTexCoord;
    if (isRing) {
        // Carve an annulus out of the quad and turn the radial distance into a
        // 1D lookup along the ring texture.
        float dist = length(vTexCoord - vec2(0.5, 0.5)) * 2.0;

        if (dist < RING_INNER || dist > RING_OUTER) {
            discard;
        }

        uv = vec2((dist - RING_INNER) / (RING_OUTER - RING_INNER), 0.5);
    }

    vec4 texColor = texture(bodyTexture, uv);
    if (texColor.a < 0.1 || (isRing && length(texColor.rgb) < 0.15)) {
        discard;
    }

    // The Sun and the sky sphere are emissive: no lighting applied.
    if (bodyType == BODY_SUN || bodyType == BODY_SKY) {
        FragColor = texColor;
        return;
    }

    vec3 normal   = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vWorldPos);
    vec3 viewDir  = normalize(cameraPos - vWorldPos);

    // Lambert (diffuse)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Blinn-Phong (specular)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3);

    vec3 ambient = vec3(0.05);

    if (isRing) {
        // Is this point of the ring in the parent planet's shadow? Project the
        // planet's centre onto the light ray reaching this fragment and compare
        // the distance from that ray with the planet's radius: a ray/cylinder
        // test, valid as long as the light is far enough to be treated as a
        // point source at the origin.
        vec3 rayDir = normalize(vWorldPos - lightPos);
        float projectedDistance = dot(parentCenter - lightPos, rayDir);
        vec3 closestPoint = lightPos + rayDir * projectedDistance;
        float distanceFromAxis = length(closestPoint - parentCenter);

        bool behindPlanet = projectedDistance < length(vWorldPos - lightPos);

        if (distanceFromAxis < (parentRadius * 0.98) && behindPlanet) {
            ambient = vec3(0.005);
            diffuse = vec3(0.0);
            texColor.rgb *= 0.15;
        } else {
            // The quad's normal is perpendicular to the ring plane while the
            // light arrives almost edge-on, so the diffuse term is near zero.
            // A high ambient term stands in for the scattering of a thin medium.
            ambient = vec3(0.8);
        }
    }

    FragColor = vec4(ambient + diffuse + specular, 1.0) * texColor;
}
