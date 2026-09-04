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

    // The alpha test only concerns the rings. It used to run for every fragment
    // of every body, where the JPEG textures are fully opaque and it could never
    // trigger, while a discard anywhere in the shader costs the whole program
    // its early depth test.
    if (isRing && (outsideAnnulus || texColor.a < 0.1 || length(texColor.rgb) < 0.15)) {
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

    // No inverse-square falloff: at true scale Neptune would receive about
    // 1/900th of Mercury's illumination and be invisible. Distance attenuation
    // is deliberately omitted so every planet stays readable.
    vec3 albedo = texColor.rgb;

    // Lambert (diffuse)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Blinn-Phong (specular), gated by the same cosine as the diffuse term.
    // The rendering equation applies the incidence factor to every reflected
    // component, not just the diffuse one; without it Blinn-Phong leaks a
    // highlight onto surfaces turned away from the light, which showed up as a
    // glint on the night side and on the shadowed part of the rings.
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3) * diff;

    // Linear-space fill light. The old value of 0.05 was tuned back when the
    // shader multiplied gamma-encoded texels and wrote the result out with no
    // further encoding. Now the albedo arrives linear and the result is encoded
    // on write, so the same constant came out roughly four times brighter on
    // screen and washed out the night side.
    vec3 ambient = vec3(0.008);

    if (isRing) {
        // Is this point of the ring in the parent planet's shadow? Project the
        // planet's centre onto the light ray reaching this fragment and compare
        // the distance from that ray with the planet's radius: a ray/cylinder
        // test, valid as long as the light is far enough to be treated as a
        // point source.
        vec3 rayDir = normalize(vWorldPos - lightPos);
        float projectedDistance = dot(parentCenter - lightPos, rayDir);
        vec3 closestPoint = lightPos + rayDir * projectedDistance;
        float distanceFromAxis = length(closestPoint - parentCenter);

        bool behindPlanet = projectedDistance < length(vWorldPos - lightPos);

        if (distanceFromAxis < (parentRadius * 0.98) && behindPlanet) {
            ambient  = vec3(0.005);
            diffuse  = vec3(0.0);
            // The planet blocks the light: there is nothing left to reflect,
            // so the highlight has to go as well.
            specular = vec3(0.0);
            albedo  *= 0.15;
        } else {
            // The quad's normal is perpendicular to the ring plane while the
            // light arrives almost edge-on, so the diffuse term is near zero.
            // A high ambient term stands in for the scattering of a thin medium.
            ambient = vec3(0.8);
        }
    }

    // Ambient and diffuse are reflected light and take the surface colour;
    // the specular highlight of a dielectric is the colour of the light source
    // itself, so it is added rather than multiplied. Folding it into the albedo
    // was what gave Mars a reddish highlight instead of a white one.
    vec3 color = (ambient + diffuse) * albedo + specular;

    FragColor = vec4(color, texColor.a);
}
