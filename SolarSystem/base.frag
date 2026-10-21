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
const int BODY_COMET_TAIL = 5;

uniform int bodyType;

uniform vec3 cameraPos;
uniform vec3 lightPos;
uniform sampler2D bodyTexture;

// Rings only: the body casting the shadow onto them.
uniform vec3  parentCenter;
uniform float parentRadius;        // equatorial
uniform float parentPolarRadius;

// Planets with rings: the ring plane, through ringCenter, and the ring texture.
uniform sampler2D ringTexture;
uniform vec3  ringCenter;
uniform vec3  ringNormal;
uniform float ringOuterRadius;   // 0 when the body has no rings

// Dynamic Eclipse Shadowing
uniform float sunRadius;
uniform int shadowCasterCount;
uniform vec3 shadowCasterPos[8];
uniform float shadowCasterRadius[8];
// Air over each caster's radius that fades sunlight out, as a fraction of it: the
// shadow edge is soft over that band, not only over the Sun's penumbra.
uniform float shadowCasterSoft[8];
// 1 when the single caster is Earth: its air widens the shadow and bends red light into it.
uniform float casterHasAtmosphere;
// 1 for bodies without an atmosphere worth the name: shaded as dusty regolith.
uniform float airless;
// Comet tails: x 0 for the gas (ion) tail with the coma, 1 for the dust tail; y how far
// the dust tail bends sideways, towards where the comet came from.
uniform vec2 cometTail;
// Earth only: cloud cover (grey = opacity, loaded linear), fixed to the ground.
uniform sampler2D cloudTexture;
uniform float hasClouds;
// Earth only: city lights (Black Marble), and the north pole to find east and north.
uniform sampler2D nightTexture;
uniform float hasNight;
uniform vec3 bodyPole;
// Limb haze colour x strength; 0 for bodies without air worth drawing.
uniform vec3 atmoColor;

// Cloud tops over the ground, as a fraction of Earth's radius: ~8 km of 6,371.
const float CLOUD_HEIGHT = 8.0 / 6371.0;

// Radial extent of the ring annulus, as a fraction of the quad's half width.
const float RING_INNER = 0.502;   // 70,750 km of the 140,920 km quad, see SolarSystem.hpp
const float RING_OUTER = 1.0;

// Area shared by two discs of radii r1, r2 whose centres are d apart.
float discOverlap(float r1, float r2, float d) {
    if (d >= r1 + r2) return 0.0;
    float rMin = min(r1, r2);
    if (d <= abs(r1 - r2)) return 3.14159265 * rMin * rMin;
    float a1 = acos(clamp((d * d + r1 * r1 - r2 * r2) / (2.0 * d * r1), -1.0, 1.0));
    float a2 = acos(clamp((d * d + r2 * r2 - r1 * r1) / (2.0 * d * r2), -1.0, 1.0));
    return r1 * r1 * (a1 - 0.5 * sin(2.0 * a1)) + r2 * r2 * (a2 - 0.5 * sin(2.0 * a2));
}

void main() {
    if (bodyType == BODY_SUN_GLOW) {
        vec3 viewDir = normalize(cameraPos - vWorldPos);
        float ndotv  = clamp(dot(normalize(vNormal), viewDir), 0.0, 1.0);
        float corona = pow(ndotv, 3.2) * 0.55;
        vec3 glowColor = mix(vec3(1.0, 0.55, 0.12), vec3(1.0, 0.96, 0.85), pow(ndotv, 1.6));
        FragColor = vec4(glowColor * corona, corona);
        return;
    }

    if (bodyType == BODY_COMET_TAIL) {
        // x from the coma's back edge (0) to the tail's end (1), the nucleus at
        // parentRadius; y across, 0 on the axis. airless carries the activity (0-1).
        float head = parentRadius;
        float x = vTexCoord.x;
        float y = abs(vTexCoord.y - 0.5) * 2.0;
        float s = max(x - head, 0.0) / (1.0 - head);          // along the tail
        vec3 c;
        float a;
        if (cometTail.x < 0.5) {
            // Gas: ions blown straight out by the solar wind, narrow, blue (CO+ glow).
            float width = 0.02 + 0.25 * sqrt(s);
            float tail = exp(-2.5 * s) * exp(-pow(y / width, 2.0) * 5.0) * smoothstep(0.0, 0.01, s);
            // Coma radius over the quad's half width: 0.012 / 0.16 in SolarSystem::drawCometTail.
            float r = length(vec2((x - head) / head, y / 0.075));
            float coma = exp(-r * r * 3.0);
            c = vec3(0.45, 0.65, 1.0) * 0.45 * tail + vec3(1.0, 0.95, 0.85) * 0.9 * coma;
            a = clamp(max(tail, coma), 0.0, 1.0);
        } else {
            // Dust: pushed gently by sunlight, it lags behind on the orbit, so the tail is
            // broad, curved and yellowish (sunlight scattered off grains).
            float yc = abs(vTexCoord.y - 0.5 - 0.5 * cometTail.y * s * s) * 2.0;
            float width = 0.05 + 0.6 * sqrt(s);
            float tail = exp(-3.5 * s) * exp(-pow(yc / width, 2.0) * 3.0) * smoothstep(0.0, 0.02, s);
            c = vec3(1.0, 0.92, 0.75) * 0.55 * tail;
            a = clamp(tail, 0.0, 1.0);
        }
        a *= airless;
        FragColor = vec4(c * airless, a);
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
    if (isRing && (outsideAnnulus || texColor.a < 0.004)) {
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
    // Airless bodies are dusty regolith or ice: Lommel-Seeliger, so a full Moon or a
    // full Io is evenly bright to its limb instead of Lambert-dark there. Cloudy
    // planets keep Lambert, whose darkening limb is what their photos show.
    if (airless > 0.5) diff = 2.0 * diff / (diff + max(dot(normal, viewDir), 0.0) + 1e-4);
    vec3 diffuse = diff * vec3(1.0);

    // Blinn-Phong (specular), gated by the same cosine as the diffuse term.
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3) * diff * (1.0 - airless);   // dust has no glint

    vec3 ambient = vec3(0.008);

    // Dynamic Eclipse Shadowing (Solar & Lunar): the fraction of the Sun's disc
    // each caster leaves visible from this point, as angular discs on the sky.
    float shadowFactor = 1.0;
    vec3 umbraLight = vec3(0.0);
    if (diff > 0.0) {
        vec3 toSun = lightPos - vWorldPos;
        float sunAngle = asin(min(sunRadius / length(toSun), 1.0));
        for (int i = 0; i < shadowCasterCount; ++i) {
            vec3 toCaster = shadowCasterPos[i] - vWorldPos;
            if (dot(toCaster, toSun) <= 0.0) continue;
            float radius = shadowCasterRadius[i];
            float soft = shadowCasterSoft[i];
            float casterDistance = length(toCaster);
            float separation = 2.0 * asin(min(0.5 * length(normalize(toCaster) - normalize(toSun)), 1.0));
            // An airless caster is a hard disc; air dims the light over a band above the
            // radius, averaged here as four discs across it. On Earth the band's mean is
            // Danjon's 1/85 widening of the shadow.
            float hidden = 0.0;
            int layers = (soft > 0.0) ? 4 : 1;
            for (int k = 0; k < layers; ++k) {
                float r = radius * (1.0 + soft * (float(k) + 0.5) / float(layers));
                hidden += discOverlap(sunAngle, asin(min(r / casterDistance, 1.0)), separation);
            }
            hidden /= float(layers) * 3.14159265 * sunAngle * sunAngle;
            float casterAngle = asin(min(radius * (1.0 + 0.5 * soft) / casterDistance, 1.0));
            shadowFactor *= 1.0 - hidden;
            if (casterHasAtmosphere > 0.5) {
                // Sunlight refracted through Earth's limb: deep red at the axis, copper
                // towards the umbra's edge. Far dimmer than sunlight, raised for the eye.
                float edge = clamp(separation / max(casterAngle - sunAngle, 1e-6), 0.0, 1.0);
                umbraLight += hidden * mix(vec3(0.07, 0.008, 0.002), vec3(0.17, 0.052, 0.015), edge * edge) * diff;
            }
        }
    }
    // On the Moon in Earth's shadow, map the lit fraction as the eye does, on a log
    // scale with 1,000:1 of contrast: the sliver of a partial eclipse, lit by 0.04-3% of
    // the Sun, then reads 5-50% bright instead of black, and the umbra's edge keeps the
    // soft ramp photos show. Seen from space, Earth keeps the linear penumbra that
    // photos of solar eclipses show.
    if (casterHasAtmosphere > 0.5) shadowFactor = log(1.0 + 1000.0 * shadowFactor) / log(1.0 + 1000.0);

    // The rings' shadow: where the ray towards the Sun crosses the ring plane, the
    // light left is what the ring texture lets through at that radius. The branch is
    // uniform per draw, so texture() keeps its derivatives for the mip level.
    if (ringOuterRadius > 0.0) {
        vec3 toSun = normalize(lightPos - vWorldPos);
        float t = dot(ringCenter - vWorldPos, ringNormal) / dot(toSun, ringNormal);
        float r = length(vWorldPos + toSun * t - ringCenter) / ringOuterRadius;
        vec4 ring = texture(ringTexture, vec2(clamp((r - RING_INNER) / (RING_OUTER - RING_INNER), 0.0, 1.0), 0.5));
        // Same cut as the ring pass: what it discards casts nothing.
        bool opaque = ring.a >= 0.004;
        if (t > 0.0 && r >= RING_INNER && r <= RING_OUTER && opaque) {
            // Sunlight crosses the ring at a slant: e^-tau/mu0, darker the lower the Sun.
            float blocked = 1.0 - exp(log(max(1.0 - ring.a, 0.002)) / max(abs(dot(toSun, ringNormal)), 0.002));
            shadowFactor *= 1.0 - blocked;
            // Ringshine: the rings themselves glow in sunlight and light the band their
            // shadow darkens, so its cloud belts still show, a few percent of the Sun.
            umbraLight += vec3(0.06) * blocked * diff;
        }
    }
    diffuse *= shadowFactor;
    specular *= shadowFactor;

    if (isRing) {
        // The planet's shadow: the ray from here to the Sun against the oblate globe,
        // in coordinates stretched along the pole (the ring normal) so the globe is a
        // sphere of the equatorial radius. Measured from the planet, so floats keep
        // metre precision instead of the Sun's 10^9 km.
        vec3 pole = normalize(vNormal);
        float stretch = parentRadius / max(parentPolarRadius, 1.0) - 1.0;
        vec3 o = vWorldPos - parentCenter;
        vec3 d = normalize(lightPos - vWorldPos);
        o += stretch * dot(o, pole) * pole;
        d += stretch * dot(d, pole) * pole;
        float tClosest = -dot(o, d) / dot(d, d);
        float miss = length(o + d * max(tClosest, 0.0));
        // Saturn's upper air dims the edge over a few hundred km.
        float inShadow = (tClosest > 0.0) ? 1.0 - smoothstep(parentRadius * 0.995, parentRadius * 1.003, miss) : 0.0;
        // A layer of particles of optical depth tau, read off the texture's opacity
        // (seen face-on, a = 1 - e^-tau). Single scattering, mu0 and mu the cosines of
        // the Sun and of the eye to the ring plane: the lit face brightens as
        // 1 - e^-tau(1/mu + 1/mu0), so with the Sun grazing (equinox) the rings fade
        // out; the far face only glows with light that got through; and the eye sees
        // through e^-tau/mu of them, less the more slanted the view.
        float a = texColor.a;
        float tau = -log(max(1.0 - a, 0.002));
        float sunUp = dot(pole, lightDir), eyeUp = dot(pole, viewDir);
        float mu0 = max(abs(sunUp), 0.002), mu = max(abs(eyeUp), 0.02);
        float litFace = mu0 / (mu + mu0) * (1.0 - exp(-tau * (1.0 / mu + 1.0 / mu0)));
        float farFace = (abs(mu - mu0) > 1e-3) ? mu0 / (mu0 - mu) * (exp(-tau / mu0) - exp(-tau / mu))
                                               : tau / mu * exp(-tau / mu);
        float sameSide = smoothstep(-0.01, 0.01, eyeUp * sign(sunUp));
        // Brightness relative to the Sun and the eye both 60 deg from the poles, the
        // geometry the texture's colours stand for.
        float glow = mix(farFace, litFace, sameSide) / max(0.5 * (1.0 - exp(-4.0 * tau)), 1e-4);
        float seen = 1.0 - exp(-tau / mu);
        // In Saturn's shadow, its dayside glow, ~1.5% of sunlight.
        vec3 radiance = albedo * 1.2 * glow * a * mix(1.0, 0.015, inShadow);
        FragColor = vec4(radiance / max(seen, 1e-3), seen);
        return;
    }

    vec3 color;
    if (hasClouds > 0.5) {
        // Earth: only open water glints (dark and blue in the Blue Marble map), clouds
        // hide the ground lit like it, and Rayleigh scattering adds the blue haze that
        // makes the oceans blue from space, thicker towards the limb.
        float water = (albedo.b > 1.5 * albedo.r && albedo.b > albedo.g && albedo.b < 0.05) ? 1.0 : 0.0;
        float cloud = texture(cloudTexture, vTexCoord).r;
        // Cloud shadow: the cloud that hides the Sun from here sits CLOUD_HEIGHT up the
        // ray towards it, so its map is read that far sunwards. The slant is capped
        // where the Sun is about to set, so the shift stays 3 texels at most at noon
        // to under 200 km at the terminator.
        float cosZ = dot(normal, lightDir);
        vec3 east = cross(bodyPole, normal);
        float cosLat = length(east);
        vec3 slant = (lightDir - normal * cosZ) / max(cosZ, 0.05) * CLOUD_HEIGHT;
        vec2 shift = vec2(dot(slant, east / max(cosLat, 1e-3)) / (6.2831853 * max(cosLat, 0.05)),
                          dot(slant, cross(normal, east / max(cosLat, 1e-3))) / 3.14159265);
        float cloudShadow = (cosZ > 0.0) ? textureLod(cloudTexture, vTexCoord + shift, 2.0).r : 0.0;
        vec3 ground = (ambient + diffuse * (1.0 - 0.55 * cloudShadow) + umbraLight) * albedo + specular * water * (1.0 - cloudShadow);
        vec3 light = ambient + diffuse + umbraLight;
        color = mix(ground, light * vec3(0.92), cloud);
        float mu = max(dot(normal, viewDir), 0.0);
        color += (diffuse + umbraLight) * vec3(0.010, 0.024, 0.065) * (1.0 + 2.0 * (1.0 - mu)) * (1.0 - 0.6 * cloud);
        // City lights on the night side, fading through twilight, dimmed under cloud.
        if (hasNight > 0.5) {
            float dark = 1.0 - smoothstep(-0.10, 0.05, cosZ);
            color += texture(nightTexture, vTexCoord).r * vec3(1.0, 0.72, 0.40) * 0.35 * dark * (1.0 - 0.85 * cloud);
        }
    } else {
        color = (ambient + diffuse + umbraLight) * albedo + specular;
    }

    // Limb haze: air seen edge-on scatters sunlight, a thin rim brightest on the lit
    // side that wraps a little past the terminator, where the high air still sees the Sun.
    if (dot(atmoColor, atmoColor) > 0.0) {
        float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
        float lit = smoothstep(-0.25, 0.35, dot(normal, lightDir)) * shadowFactor;
        color += atmoColor * rim * lit;
    }

    FragColor = vec4(color, texColor.a);
}
