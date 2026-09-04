#pragma once
#include "../glad/gl.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <string>

class Texture {
public:
    GLuint ID = 0;

    Texture(const std::string& path) {

        sf::Image image;
        if (!image.loadFromFile(path)) {
            std::cerr << "ERRORE: Impossibile caricare la texture in: " << path << std::endl;
            ID = 0;
            return;
        }
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

    // La texture possiede un handle OpenGL: una copia lo cancellerebbe due volte.
    // Lo spostamento e' consentito e svuota l'oggetto sorgente.
    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& altra) noexcept : ID(altra.ID) {
        altra.ID = 0;
    }

    Texture& operator=(Texture&& altra) noexcept {
        if (this != &altra) {
            if (ID != 0) glDeleteTextures(1, &ID);
            ID = altra.ID;
            altra.ID = 0;
        }
        return *this;
    }

    void bind(unsigned int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, ID);
    }
};
