#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>

// Anisotropic filtering is an extension in OpenGL 3.3 (core only from 4.6), and
// the glad loader here was generated without extensions, so the two enums are
// spelled out. Support is probed at runtime rather than assumed.
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

class Texture {
public:
    GLuint ID = 0;

    explicit Texture(const std::string& path) {

        sf::Image image;
        if (!image.loadFromFile(path)) {
            std::cerr << "ERROR: could not load texture at: " << path << std::endl;
            ID = 0;
            return;
        }
        // Image files store the top row first, OpenGL expects the bottom row
        // first, so the rows are flipped once at load time.
        image.flipVertically();

        glGenTextures(1, &ID);
        glBindTexture(GL_TEXTURE_2D, ID);
        // Longitude wraps, latitude does not. With GL_REPEAT on T the filter
        // blended the top row of the map into the bottom one, smearing the
        // south pole across the north and leaving a discoloured ring at both.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Where the meridians of a UV sphere converge, one texel footprint
        // covers a wide, razor-thin strip. An isotropic filter has to pick a
        // single mip level for both axes, and the compromise is what draws the
        // radial streaks at the poles. Anisotropic filtering samples along the
        // long axis instead and removes them.
        const float anisotropy = maxAnisotropy();
        if (anisotropy > 1.0f) {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
        }

        sf::Vector2u size = image.getSize();
        const GLvoid* pixelData = image.getPixelsPtr();

        // GL_SRGB8_ALPHA8, not GL_RGBA: photographic textures are authored in
        // sRGB, so sampling them raw meant doing the lighting maths on
        // non-linear values. The hardware now decodes each texel to linear on
        // sampling, and GL_FRAMEBUFFER_SRGB re-encodes on write. Only the three
        // colour channels are converted; alpha stays linear, which is what the
        // ring cutout needs.
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~Texture() {
        if (ID != 0) {
            glDeleteTextures(1, &ID);
        }
    }

    // The texture owns an OpenGL handle: a copy would delete it twice.
    // Moving is allowed and leaves the source object empty.
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept : ID(other.ID) {
        other.ID = 0;
    }

    Texture& operator=(Texture&& other) noexcept {
        if (this != &other) {
            if (ID != 0) glDeleteTextures(1, &ID);
            ID = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    // Queried once. If the extension is missing the query raises GL_INVALID_ENUM
    // and leaves the value at 1, which disables the code path.
    static float maxAnisotropy() {
        static const float value = [] {
            GLfloat limit = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &limit);
            while (glGetError() != GL_NO_ERROR) { }
            return (limit > 1.0f) ? limit : 1.0f;
        }();
        return value;
    }

    void bind(unsigned int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, ID);
    }
};
