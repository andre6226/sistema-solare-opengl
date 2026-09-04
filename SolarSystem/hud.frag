#version 330 core
in vec2 vTexCoord;
in vec4 vColor;

out vec4 FragColor;

// 0 = solid quad (panels, rules), 1 = glyph sampled from the font atlas
uniform int mode;
uniform sampler2D fontAtlas;

const int MODE_SOLID = 0;

void main() {
    if (mode == MODE_SOLID) {
        FragColor = vColor;
        return;
    }

    // SFML rasterises glyphs as white texels whose alpha carries the coverage,
    // so only the alpha channel is meaningful here.
    float coverage = texture(fontAtlas, vTexCoord).a;
    FragColor = vec4(vColor.rgb, vColor.a * coverage);
}
