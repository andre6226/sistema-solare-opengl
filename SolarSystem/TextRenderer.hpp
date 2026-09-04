#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Image.hpp>
#include <glm/glm.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "Shader.hpp"

// Immediate-mode 2D overlay renderer built entirely on the core profile.
//
// SFML is used here ONLY to rasterise glyphs on the CPU: its 2D renderer is
// never invoked. That matters because sf::RenderWindow::draw() relies on state
// that a core profile does not have, and pushGLStates()/popGLStates() does not
// round-trip everything it touches (the depth test and the bound vertex array
// among them). On Linux and Windows the drivers are lenient enough to hide it;
// macOS exposes no compatibility profile at all above OpenGL 3.2, so there the
// depth buffer really does come back broken. Owning the draw path removes the
// problem rather than working around it.
//
// Quads are accumulated across a frame and flushed in two draw calls: one for
// solid rectangles, one for text.
class TextRenderer {
public:
    TextRenderer(const std::string& fontPath, unsigned int pixelHeight)
        : shader("SolarSystem/hud.vert", "SolarSystem/hud.frag"), glyphHeight(pixelHeight) {

        if (!font.openFromFile(fontPath)) {
            throw std::runtime_error("could not open the HUD font at '" + fontPath + "'");
        }

        buildAtlas();

        locScreenSize = shader.uniformLocation("screenSize");
        locMode       = shader.uniformLocation("mode");

        shader.use();
        glUniform1i(shader.uniformLocation("fontAtlas"), 0);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // position (2) + uv (2) + colour (4)
        const GLsizei stride = 8 * sizeof(float);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    ~TextRenderer() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        if (atlas != 0) glDeleteTextures(1, &atlas);
    }

    TextRenderer(const TextRenderer&)            = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
    TextRenderer(TextRenderer&&)                 = delete;
    TextRenderer& operator=(TextRenderer&&)      = delete;

    // Saves the pieces of 3D state the overlay disturbs and switches to 2D.
    void begin(unsigned int width, unsigned int height) {
        screenWidth  = static_cast<float>(width);
        screenHeight = static_cast<float>(height);

        solidQuads.clear();
        textQuads.clear();

        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        // Interface colours are authored in sRGB and must reach the framebuffer
        // untouched, so the automatic linear-to-sRGB encode is switched off for
        // the overlay and restored in end().
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    void end() {
        shader.use();
        glUniform2f(locScreenSize, screenWidth, screenHeight);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);

        flush(solidQuads, 0);
        if (!textQuads.empty()) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, atlas);
            flush(textQuads, 1);
        }

        glBindVertexArray(0);

        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

    void rect(float x, float y, float w, float h, const glm::vec4& color) {
        pushQuad(solidQuads, x, y, w, h, glm::vec2(0.0f), glm::vec2(0.0f), color);
    }

    // Draws `text` with its left edge at x and its BASELINE at y.
    void text(const std::string& utf8, float x, float y, float scale, const glm::vec4& color) {
        float penX = x;
        std::uint32_t previous = 0;

        for (std::uint32_t codePoint : decodeUtf8(utf8)) {
            const auto found = glyphs.find(codePoint);
            if (found == glyphs.end()) {
                previous = codePoint;
                continue;
            }
            const GlyphInfo& g = found->second;

            if (previous != 0) {
                penX += font.getKerning(previous, codePoint, glyphHeight) * scale;
            }

            if (g.size.x > 0.0f && g.size.y > 0.0f) {
                pushQuad(textQuads,
                         penX + g.offset.x * scale, y + g.offset.y * scale,
                         g.size.x * scale, g.size.y * scale,
                         g.uvMin, g.uvMax, color);
            }

            penX += g.advance * scale;
            previous = codePoint;
        }
    }

    float measure(const std::string& utf8, float scale) const {
        float width = 0.0f;
        std::uint32_t previous = 0;

        for (std::uint32_t codePoint : decodeUtf8(utf8)) {
            const auto found = glyphs.find(codePoint);
            if (found == glyphs.end()) continue;

            if (previous != 0) {
                width += font.getKerning(previous, codePoint, glyphHeight) * scale;
            }
            width += found->second.advance * scale;
            previous = codePoint;
        }
        return width;
    }

    float lineHeight(float scale) const {
        return font.getLineSpacing(glyphHeight) * scale;
    }

private:
    struct GlyphInfo {
        glm::vec2 uvMin{0.0f};
        glm::vec2 uvMax{0.0f};
        glm::vec2 size{0.0f};   // pixels at the baked size
        glm::vec2 offset{0.0f}; // from the pen position / baseline
        float advance = 0.0f;
    };

    Shader shader;
    sf::Font font;
    unsigned int glyphHeight;
    GLuint atlas = 0;
    GLuint VAO = 0, VBO = 0;
    GLint locScreenSize = -1, locMode = -1;
    float screenWidth = 1.0f, screenHeight = 1.0f;

    std::unordered_map<std::uint32_t, GlyphInfo> glyphs;
    std::vector<float> solidQuads;
    std::vector<float> textQuads;

    // Printable ASCII plus the two typographic signs the HUD uses.
    static std::vector<std::uint32_t> characterSet() {
        std::vector<std::uint32_t> set;
        for (std::uint32_t c = 32; c < 127; ++c) set.push_back(c);
        set.push_back(0x00B0); // degree sign
        set.push_back(0x00D7); // multiplication sign
        return set;
    }

    void buildAtlas() {
        const std::vector<std::uint32_t> wanted = characterSet();

        // Every glyph has to be requested BEFORE the atlas texture is read back:
        // SFML grows and repacks it on demand, so a copy taken early would be
        // stale the moment another character is asked for.
        for (std::uint32_t codePoint : wanted) {
            (void)font.getGlyph(codePoint, glyphHeight, false);
        }

        const sf::Image image = font.getTexture(glyphHeight).copyToImage();
        const sf::Vector2u size = image.getSize();

        glGenTextures(1, &atlas);
        glBindTexture(GL_TEXTURE_2D, atlas);
        // GL_RGBA8, deliberately not an sRGB format: these texels are a coverage
        // mask, not colour, and must not go through a gamma decode.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, image.getPixelsPtr());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        const float atlasW = static_cast<float>(size.x);
        const float atlasH = static_cast<float>(size.y);

        for (std::uint32_t codePoint : wanted) {
            const sf::Glyph& glyph = font.getGlyph(codePoint, glyphHeight, false);

            GlyphInfo info;
            info.advance = glyph.advance;
            info.offset  = glm::vec2(glyph.bounds.position.x, glyph.bounds.position.y);
            info.size    = glm::vec2(glyph.bounds.size.x, glyph.bounds.size.y);
            info.uvMin   = glm::vec2(glyph.textureRect.position.x / atlasW,
                                     glyph.textureRect.position.y / atlasH);
            info.uvMax   = glm::vec2((glyph.textureRect.position.x + glyph.textureRect.size.x) / atlasW,
                                     (glyph.textureRect.position.y + glyph.textureRect.size.y) / atlasH);

            glyphs.emplace(codePoint, info);
        }
    }

    static std::vector<std::uint32_t> decodeUtf8(const std::string& input) {
        std::vector<std::uint32_t> out;
        std::size_t i = 0;

        while (i < input.size()) {
            const unsigned char lead = static_cast<unsigned char>(input[i]);
            std::uint32_t codePoint = lead;
            std::size_t extra = 0;

            if (lead >= 0xF0)      { codePoint = lead & 0x07u; extra = 3; }
            else if (lead >= 0xE0) { codePoint = lead & 0x0Fu; extra = 2; }
            else if (lead >= 0xC0) { codePoint = lead & 0x1Fu; extra = 1; }

            if (i + extra >= input.size()) extra = 0;

            for (std::size_t k = 1; k <= extra; ++k) {
                codePoint = (codePoint << 6) | (static_cast<unsigned char>(input[i + k]) & 0x3Fu);
            }

            out.push_back(codePoint);
            i += extra + 1;
        }
        return out;
    }

    static void pushQuad(std::vector<float>& into, float x, float y, float w, float h,
                         const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec4& c) {
        const float x0 = x, y0 = y, x1 = x + w, y1 = y + h;
        const float u0 = uvMin.x, v0 = uvMin.y, u1 = uvMax.x, v1 = uvMax.y;

        const float quad[6][8] = {
            {x0, y0, u0, v0, c.r, c.g, c.b, c.a},
            {x0, y1, u0, v1, c.r, c.g, c.b, c.a},
            {x1, y1, u1, v1, c.r, c.g, c.b, c.a},
            {x0, y0, u0, v0, c.r, c.g, c.b, c.a},
            {x1, y1, u1, v1, c.r, c.g, c.b, c.a},
            {x1, y0, u1, v0, c.r, c.g, c.b, c.a},
        };

        into.insert(into.end(), &quad[0][0], &quad[0][0] + 6 * 8);
    }

    void flush(const std::vector<float>& vertices, int mode) {
        if (vertices.empty()) return;

        glUniform1i(locMode, mode);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 8));
    }
};
