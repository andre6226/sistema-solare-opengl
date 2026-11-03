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
// shortcut: one haze depth for every atmosphere, sets only how thin the limb glow is; give
// each planet its own if Venus's thick haze and Mars's thin dust must differ in width too.
const float HAZE_TAU = 0.05;
// How far twilight reaches past the terminator, as the sine of the Sun's depth: the glow
// falls by e every ~3 degrees. shortcut: one value for every atmosphere, set it per planet
// from its scale height if Venus's and Mars's twilight must differ from Earth's.
const float TWILIGHT = 0.05;

// Cloud tops over the ground, as a fraction of Earth's radius: ~8 km of 6,371.
const float CLOUD_HEIGHT = 8.0 / 6371.0;

// Radial extent of the ring annulus, as a fraction of the quad's half width.
// Where each ring texture starts, and below which the ring is dust (forward-scattering),
// as fractions of its outer radius: CelestialBody::setRingProfile.
uniform float ringInner;
uniform float ringDustBelow;
const float RING_OUTER = 1.0;

// The photosphere's colour in linear sRGB: a 5772 K black body through the CIE 1931
// observer, white point D65 (peach-white, #FFF1EA on screen).
const vec3 SUN_COLOR = vec3(1.0, 0.879, 0.822);

// <umbra-table>
// tools/umbra_light.py: per km of tangent height 0-60 km, sunlight through Earth's
// limb (linear sRGB, sunlight = 1) and its total refraction (rad). Generated, do not edit.
const int UMBRA_ROWS = 61;
const vec4 UMBRA_LIMB[UMBRA_ROWS] = vec4[](
    vec4(0.0000e+00, 0.0000e+00, 0.0000e+00, 1.893084e-02),
    vec4(0.0000e+00, 0.0000e+00, 0.0000e+00, 1.747702e-02),
    vec4(0.0000e+00, 0.0000e+00, 0.0000e+00, 1.592543e-02),
    vec4(1.3026e-04, 9.2729e-06, 0.0000e+00, 1.448782e-02),
    vec4(1.3283e-03, 1.5935e-04, 0.0000e+00, 1.315838e-02),
    vec4(4.9175e-03, 8.3621e-04, 0.0000e+00, 1.192897e-02),
    vec4(1.1636e-02, 2.6068e-03, 0.0000e+00, 1.079634e-02),
    vec4(2.1603e-02, 6.1438e-03, 0.0000e+00, 9.754559e-03),
    vec4(3.4440e-02, 1.2173e-02, 0.0000e+00, 8.801272e-03),
    vec4(4.9264e-02, 2.1356e-02, 1.2606e-03, 7.937532e-03),
    vec4(6.4736e-02, 3.4121e-02, 5.0428e-03, 7.175842e-03),
    vec4(7.9312e-02, 5.0595e-02, 1.2977e-02, 6.761356e-03),
    vec4(9.1322e-02, 7.0230e-02, 2.7355e-02, 5.761370e-03),
    vec4(9.8829e-02, 9.0362e-02, 4.8919e-02, 4.899375e-03),
    vec4(1.0124e-01, 1.0848e-01, 7.7001e-02, 4.169351e-03),
    vec4(1.0082e-01, 1.2459e-01, 1.1088e-01, 3.550367e-03),
    vec4(1.0054e-01, 1.4061e-01, 1.5109e-01, 3.025001e-03),
    vec4(1.0140e-01, 1.5699e-01, 1.9678e-01, 2.578808e-03),
    vec4(1.0434e-01, 1.7444e-01, 2.4685e-01, 2.199845e-03),
    vec4(1.1034e-01, 1.9385e-01, 3.0013e-01, 1.878461e-03),
    vec4(1.2037e-01, 2.1629e-01, 3.5551e-01, 1.614441e-03),
    vec4(1.3553e-01, 2.4280e-01, 4.1186e-01, 1.368860e-03),
    vec4(1.5702e-01, 2.7428e-01, 4.6760e-01, 1.161254e-03),
    vec4(1.8612e-01, 3.1154e-01, 5.2180e-01, 9.860604e-04),
    vec4(2.2403e-01, 3.5509e-01, 5.7364e-01, 8.380573e-04),
    vec4(2.7158e-01, 4.0493e-01, 6.2243e-01, 7.129122e-04),
    vec4(3.2893e-01, 4.6045e-01, 6.6763e-01, 6.069799e-04),
    vec4(3.9519e-01, 5.2034e-01, 7.0887e-01, 5.172551e-04),
    vec4(4.6832e-01, 5.8263e-01, 7.4597e-01, 4.411978e-04),
    vec4(5.4517e-01, 6.4495e-01, 7.7894e-01, 3.767044e-04),
    vec4(6.2196e-01, 7.0482e-01, 8.0797e-01, 3.220408e-04),
    vec4(6.9490e-01, 7.6002e-01, 8.3339e-01, 2.758478e-04),
    vec4(7.6081e-01, 8.0893e-01, 8.5561e-01, 2.389576e-04),
    vec4(8.1759e-01, 8.5062e-01, 8.7503e-01, 2.023698e-04),
    vec4(8.6429e-01, 8.8481e-01, 8.9174e-01, 1.716050e-04),
    vec4(9.0114e-01, 9.1195e-01, 9.0612e-01, 1.458045e-04),
    vec4(9.2912e-01, 9.3290e-01, 9.1852e-01, 1.241186e-04),
    vec4(9.4967e-01, 9.4870e-01, 9.2922e-01, 1.058549e-04),
    vec4(9.6433e-01, 9.6042e-01, 9.3846e-01, 9.044132e-05),
    vec4(9.7455e-01, 9.6902e-01, 9.4645e-01, 7.740734e-05),
    vec4(9.8155e-01, 9.7532e-01, 9.5335e-01, 6.636042e-05),
    vec4(9.8630e-01, 9.7997e-01, 9.5932e-01, 5.698268e-05),
    vec4(9.8953e-01, 9.8344e-01, 9.6448e-01, 4.900210e-05),
    vec4(9.9174e-01, 9.8608e-01, 9.6895e-01, 4.219683e-05),
    vec4(9.9329e-01, 9.8815e-01, 9.7281e-01, 3.637681e-05),
    vec4(9.9442e-01, 9.8980e-01, 9.7616e-01, 3.138095e-05),
    vec4(9.9526e-01, 9.9114e-01, 9.7906e-01, 2.705584e-05),
    vec4(9.9591e-01, 9.9226e-01, 9.8156e-01, 2.299052e-05),
    vec4(9.9644e-01, 9.9319e-01, 9.8371e-01, 2.021887e-05),
    vec4(9.9688e-01, 9.9400e-01, 9.8561e-01, 1.778590e-05),
    vec4(9.9725e-01, 9.9471e-01, 9.8728e-01, 1.561435e-05),
    vec4(9.9758e-01, 9.9533e-01, 9.8875e-01, 1.348581e-05),
    vec4(9.9786e-01, 9.9586e-01, 9.9003e-01, 1.205276e-05),
    vec4(9.9810e-01, 9.9634e-01, 9.9118e-01, 1.077163e-05),
    vec4(9.9833e-01, 9.9677e-01, 9.9220e-01, 9.615177e-06),
    vec4(9.9852e-01, 9.9715e-01, 9.9312e-01, 8.572442e-06),
    vec4(9.9870e-01, 9.9749e-01, 9.9394e-01, 7.633263e-06),
    vec4(9.9886e-01, 9.9779e-01, 9.9466e-01, 6.788357e-06),
    vec4(9.9899e-01, 9.9806e-01, 9.9531e-01, 6.029111e-06),
    vec4(9.9912e-01, 9.9830e-01, 9.9589e-01, 5.347661e-06),
    vec4(9.9923e-01, 9.9851e-01, 9.9640e-01, 4.736833e-06)
);
// </umbra-table>

// Contrast the eye keeps on the eclipsed Moon, a little more than a photo's 1,000:1 so
// the umbra is not black.
const float EYE_CONTRAST = 3.0e3;

// Sunlight Earth's air bends into its shadow (RGB share of full sunlight), at a point
// that sees Earth's centre rho rad from the Sun's, Earth d km away and radius radiusKm,
// the Sun alpha rad in radius. Radiance is conserved along a ray, so the point gets the
// solid angle of limb that, traced back and bent by the table's refraction, lands on the
// Sun's disc: for each tangent height an arc of the limb ring, found in closed form.
vec3 umbraRefracted(float rho, float d, float radiusKm, float alpha) {
    vec3 e = vec3(0.0);
    for (int h = 0; h < UMBRA_ROWS; ++h) {
        float rh = radiusKm + float(h);
        float b = UMBRA_LIMB[h].w - asin(min(rh / d, 1.0));
        float arc;
        if (rho * abs(b) < 1e-12) {
            arc = (rho * rho + b * b <= alpha * alpha) ? 6.2831853 : 0.0;
        } else {
            float c = (alpha * alpha - rho * rho - b * b) / (2.0 * rho * abs(b));
            arc = 6.2831853 - 2.0 * acos(clamp(c, -1.0, 1.0));
        }
        e += UMBRA_LIMB[h].rgb * (arc * rh / (d * d * 3.14159265 * alpha * alpha));
    }
    return e;
}

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
        // Only around the disc: in front of it the glow would wash the photosphere out.
        vec3 ray = normalize(vWorldPos - cameraPos);
        if (length(cross(lightPos - cameraPos, ray)) < sunRadius) discard;
        // The corona and glare are scattered sunlight: the photosphere's colour, not orange.
        FragColor = vec4(SUN_COLOR * corona, corona);
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
        outsideAnnulus = (dist < ringInner || dist > RING_OUTER);
        uv = vec2(clamp((dist - ringInner) / (RING_OUTER - ringInner), 0.0, 1.0), 0.5);
    }

    // Sampled before any discard: texture() picks its mip level from the
    // derivatives of uv across neighbouring fragments, and those are only
    // well defined while all of them are still alive.
    vec4 texColor = texture(bodyTexture, uv);

    // The alpha test only concerns the rings.
    if (isRing && (outsideAnnulus || texColor.a < 0.002)) {
        discard;
    }

    if (bodyType == BODY_SKY) {
        FragColor = texColor;
        return;
    }

    if (bodyType == BODY_SUN) {
        vec3 viewDir = normalize(cameraPos - vWorldPos);
        float ndotv  = clamp(dot(normalize(vNormal), viewDir), 0.0, 1.0);
        // The texture is false-colour orange; keep only its granulation, as brightness,
        // with the 47% rms contrast of the map cut to ~12% (white-light granulation).
        float gran = 1.0 + 0.25 * (dot(texColor.rgb, vec3(0.2126, 0.7152, 0.0722)) / 0.406 - 1.0);
        // Limb darkening, stronger in blue, so the edge turns dimmer and redder.
        // shortcut: linear law with rough coefficients per channel, use Neckel & Labs (1994) polynomials if the limb profile matters.
        vec3 limb = vec3(1.0) - vec3(0.58, 0.67, 0.78) * (1.0 - ndotv);
        FragColor = vec4(SUN_COLOR * limb * gran, 1.0);
        return;
    }

    vec3 normal   = normalize(vNormal);
    vec3 lightDir = normalize(lightPos - vWorldPos);
    vec3 viewDir  = normalize(cameraPos - vWorldPos);

    // Sunlight falls as 1/d^2; the eye adapts to the light where the camera is (not
    // nearer the Sun than 0.2 AU), so what it looks at keeps today's exposure and a far
    // planet in the same view is as much dimmer as it really is: Saturn from near Earth
    // gets 1/90 of Earth's light.
    float sunlight = dot(cameraPos - lightPos, cameraPos - lightPos) / dot(vWorldPos - lightPos, vWorldPos - lightPos);
    sunlight = max(sunlight, 0.04 * 2.2347e16 / dot(vWorldPos - lightPos, vWorldPos - lightPos));   // (0.2 AU)^2 in km^2 floor

    vec3 albedo = texColor.rgb;

    // Lambert (diffuse). Under air, the high air still lights the ground a little past
    // the terminator: the Sun's cosine goes soft, TWILIGHT ln(1 + e^(cos/TWILIGHT)), the
    // cosine by day, fading as e^(cos/TWILIGHT) into the night, with no kink at the
    // terminator to show as a line.
    float cosSun = dot(normal, lightDir);
    float diff = (dot(atmoColor, atmoColor) > 0.0) ? TWILIGHT * log(1.0 + exp(cosSun / TWILIGHT)) : max(cosSun, 0.0);
    // Airless bodies are dusty regolith or ice: Lommel-Seeliger, so a full Moon or a
    // full Io is evenly bright to its limb instead of Lambert-dark there. Cloudy
    // planets keep Lambert, whose darkening limb is what their photos show.
    if (airless > 0.5) diff = 2.0 * diff / (diff + max(dot(normal, viewDir), 0.0) + 1e-4);
    vec3 diffuse = diff * vec3(sunlight);

    // Blinn-Phong (specular), gated by the same cosine as the diffuse term.
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3) * diff * (1.0 - airless) * sunlight;   // dust has no glint

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
            if (casterHasAtmosphere > 0.5) umbraLight = umbraRefracted(separation, casterDistance, radius, sunAngle);
        }
    }
    // On the Moon in Earth's shadow, map the light (direct + bent in) as the eye does, on
    // a log scale of its luminance, keeping its colour: the sliver of a partial eclipse,
    // lit by 0.04-3% of the Sun, reads 10-55% bright instead of black, the umbra's edge
    // keeps the soft ramp photos show, and the umbra (1e-5 to 1e-3 of sunlight) is dark
    // but not black. Seen from space, Earth keeps the linear penumbra of solar eclipse photos.
    if (casterHasAtmosphere > 0.5) {
        float lum = shadowFactor + dot(umbraLight, vec3(0.2126, 0.7152, 0.0722));
        float scale = lum > 0.0 ? log(1.0 + EYE_CONTRAST * lum) / (log(1.0 + EYE_CONTRAST) * lum) : 0.0;
        shadowFactor *= scale;
        umbraLight *= scale * diff * sunlight;
    }

    // The rings' shadow: where the ray towards the Sun crosses the ring plane, the
    // light left is what the ring texture lets through at that radius. The branch is
    // uniform per draw, so texture() keeps its derivatives for the mip level.
    if (ringOuterRadius > 0.0) {
        vec3 toSun = normalize(lightPos - vWorldPos);
        float t = dot(ringCenter - vWorldPos, ringNormal) / dot(toSun, ringNormal);
        float r = length(vWorldPos + toSun * t - ringCenter) / ringOuterRadius;
        vec4 ring = texture(ringTexture, vec2(clamp((r - ringInner) / (RING_OUTER - ringInner), 0.0, 1.0), 0.5));
        // Same cut as the ring pass: what it discards casts nothing.
        bool opaque = ring.a >= 0.002;
        if (t > 0.0 && r >= ringInner && r <= RING_OUTER && opaque) {
            // Sunlight crosses the ring at a slant: e^-tau/mu0, darker the lower the Sun.
            float blocked = 1.0 - exp(log(max(1.0 - ring.a, 0.002)) / max(abs(dot(toSun, ringNormal)), 0.002));
            shadowFactor *= 1.0 - blocked;
            // Ringshine: the rings themselves glow in sunlight and light the band their
            // shadow darkens, so its cloud belts still show, a few percent of the Sun.
            umbraLight += vec3(0.06) * blocked * diff * sunlight;
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
        // Phase: cos of the angle at the ring between Sun and eye. Packed icy particles
        // brighten sharply with the Sun right behind the eye (opposition surge, ~40%
        // within a few degrees); dust, micron grains, throws light forwards and only
        // shows with the Sun nearly behind the ring, as the D ring in Cassini's backlit
        // views: tens of times brighter there than at the reference geometry.
        float phase = acos(clamp(dot(lightDir, viewDir), -1.0, 1.0));
        float dust = step(length(vTexCoord - vec2(0.5)) * 2.0, ringDustBelow);
        glow *= mix(1.0 + 0.4 * exp(-phase / 0.05), 0.3 + 40.0 * exp(-(3.14159265 - phase) / 0.15), dust);
        // Saturnshine: the day side of the globe lights the rings from within their
        // plane, on both faces alike: the planet's albedo (0.34) times the half of its
        // disc above the ring plane and the share of it lit as seen from here, arriving
        // at a grazing mean cosine of 0.42 R/d. A percent or two of sunlight near the
        // globe, falling off as (R/d)^3.
        float dc = max(length(o), parentRadius);
        float planetLit = 0.5 * (1.0 + dot(-normalize(o), normalize(lightPos - parentCenter)));
        float mu0s = 0.42 * parentRadius / dc;
        float shine = 0.34 * planetLit * 0.5 * (parentRadius * parentRadius) / (dc * dc)
                    * mu0s / (mu + mu0s) * (1.0 - exp(-tau * (1.0 / mu + 1.0 / mu0s))) / max(0.5 * (1.0 - exp(-4.0 * tau)), 1e-4);
        // In Saturn's shadow, sunlight gives way to Saturnshine alone.
        vec3 radiance = albedo * 1.2 * (glow * (1.0 - inShadow) + shine) * a * sunlight;
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

    // Limb haze: a thin layer of air (optical depth HAZE_TAU) scattering sunlight once,
    // as mu0 / (mu0 + mu) (1 - e^-tau(1/mu + 1/mu0)), mu0 and mu the cosines of the Sun
    // and of the eye: bright at the limb, faint face-on. mu0 is the soft cosine of the
    // ground's light, so the glow falls with it, smoothly, through the twilight: no edge
    // or band of its own.
    if (dot(atmoColor, atmoColor) > 0.0) {
        float mu = max(dot(normal, viewDir), 0.01);
        float mu0 = diff;
        float haze = mu0 / (mu0 + mu) * (1.0 - exp(-HAZE_TAU * (1.0 / mu + 1.0 / max(mu0, 1e-4))));
        color += atmoColor * haze * shadowFactor * sunlight;
    }

    FragColor = vec4(color, texColor.a);
}
