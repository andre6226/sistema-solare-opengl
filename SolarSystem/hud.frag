#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

// 0 = solid quad (panels, rules, or radial beacon dots when vTexCoord != 0),
// 1 = glyph sampled from the font atlas
uniform int mode;
uniform sampler2D fontAtlas;

const int MODE_SOLID = 0;

void main() {
    if (mode == MODE_SOLID) {
        if (dot(vTexCoord, vTexCoord) > 0.0) {
            float r = length(vTexCoord);
            if (r > 1.0) discard;
            float core = exp(-r * r * 7.5);
            float glow = (1.0 - smoothstep(0.0, 1.0, r)) * 0.75;
            vec3 color = mix(vColor.rgb, vec3(1.0), core * 0.70);
            float alpha = vColor.a * clamp(core + glow, 0.0, 1.0);
            FragColor = vec4(color, alpha);
            return;
        }
        FragColor = vColor;
        return;
    }

    // SFML rasterises glyphs as white texels whose alpha carries the coverage,
    // so only the alpha channel is meaningful here.
    float coverage = texture(fontAtlas, vTexCoord).a;
    FragColor = vec4(vColor.rgb, vColor.a * coverage);
}
