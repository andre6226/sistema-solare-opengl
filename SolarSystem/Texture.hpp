#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>

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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        sf::Vector2u size = image.getSize();
        const GLvoid* pixelData = image.getPixelsPtr();

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);

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

    void bind(unsigned int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, ID);
    }
};
