#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include "Shader.hpp"

// The main belt as points: ~58,000 numbered asteroids (tools/asteroid_belt.py), each
// on its own Kepler ellipse about the Sun, solved per vertex. A picture of the belt,
// not an ephemeris: no planet perturbs these orbits.
class AsteroidBelt {
    GLuint VAO = 0, VBO = 0;
    GLsizei count = 0;
    double epochJd = 0.0;
    std::unique_ptr<Shader> shader;
    GLint locView = -1, locProjection = -1, locSun = -1, locDays = -1, locEcliptic = -1, locAlpha = -1;

public:
    explicit AsteroidBelt(const char* path) {
        std::ifstream in(path, std::ios::binary);
        char magic[8] = {};
        std::int32_t n = 0;
        in.read(magic, 8);
        in.read(reinterpret_cast<char*>(&epochJd), 8);
        in.read(reinterpret_cast<char*>(&n), 4);
        if (!in || std::memcmp(magic, "BELTKEP1", 8) != 0 || n <= 0) {
            std::cerr << "[belt] " << path << " missing or invalid: no asteroid belt" << std::endl;
            return;
        }
        std::vector<float> elements(size_t(n) * 7);
        in.read(reinterpret_cast<char*>(elements.data()), std::streamsize(elements.size() * sizeof(float)));
        if (!in) {
            std::cerr << "[belt] " << path << " truncated: no asteroid belt" << std::endl;
            return;
        }
        try {
            shader = std::make_unique<Shader>("SolarSystem/belt.vert", "SolarSystem/belt.frag");
        } catch (const std::exception& e) {
            std::cerr << "[belt] " << e.what() << std::endl;
            return;
        }
        locView = shader->uniformLocation("view");
        locProjection = shader->uniformLocation("projection");
        locSun = shader->uniformLocation("sunPos");
        locDays = shader->uniformLocation("daysSinceEpoch");
        locEcliptic = shader->uniformLocation("eclipticToWorld");
        locAlpha = shader->uniformLocation("beltAlpha");

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(elements.size() * sizeof(float)), elements.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        count = n;
    }

    ~AsteroidBelt() {
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    }

    AsteroidBelt(const AsteroidBelt&)            = delete;
    AsteroidBelt& operator=(const AsteroidBelt&) = delete;

    GLsizei size() const { return count; }

    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& sunPos,
              double jdTdb, const glm::mat3& eclipticToWorld, float alpha) const {
        if (count == 0) return;
        shader->use();
        glUniformMatrix4fv(locView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, &projection[0][0]);
        glUniform3fv(locSun, 1, &sunPos[0]);
        glUniform1f(locDays, float(jdTdb - epochJd));
        glUniformMatrix3fv(locEcliptic, 1, GL_FALSE, &eclipticToWorld[0][0]);
        glUniform1f(locAlpha, alpha);
        glDepthMask(GL_FALSE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, count);
        glBindVertexArray(0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
    }
};
