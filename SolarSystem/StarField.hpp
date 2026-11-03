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

// The naked-eye stars as points: the Yale Bright Star Catalogue (tools/star_catalog.py),
// each at its J2000 direction moved by its proper motion, as bright as its magnitude
// and coloured as its temperature. Drawn at infinity before the Milky Way picture.
// shortcut: no annual aberration (20") or parallax (< 1" but for alpha Centauri), add
// them if the sky must hold to arcseconds.
class StarField {
    GLuint VAO = 0, VBO = 0;
    GLsizei count = 0;
    std::unique_ptr<Shader> shader;
    GLint locView = -1, locProjection = -1, locIcrf = -1, locYears = -1;

public:
    explicit StarField(const char* path) {
        std::ifstream in(path, std::ios::binary);
        char magic[8] = {};
        std::int32_t n = 0;
        in.read(magic, 8);
        in.read(reinterpret_cast<char*>(&n), 4);
        if (!in || std::memcmp(magic, "STARS001", 8) != 0 || n <= 0) {
            std::cerr << "[stars] " << path << " missing or invalid: no star catalogue" << std::endl;
            return;
        }
        std::vector<float> stars(size_t(n) * 10);
        in.read(reinterpret_cast<char*>(stars.data()), std::streamsize(stars.size() * sizeof(float)));
        if (!in) {
            std::cerr << "[stars] " << path << " truncated: no star catalogue" << std::endl;
            return;
        }
        try {
            shader = std::make_unique<Shader>("SolarSystem/stars.vert", "SolarSystem/stars.frag");
        } catch (const std::exception& e) {
            std::cerr << "[stars] " << e.what() << std::endl;
            return;
        }
        locView = shader->uniformLocation("view");
        locProjection = shader->uniformLocation("projection");
        locIcrf = shader->uniformLocation("icrfToWorld");
        locYears = shader->uniformLocation("yearsSinceJ2000");

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(stars.size() * sizeof(float)), stars.data(), GL_STATIC_DRAW);
        const GLsizei stride = 10 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);                     // direction
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));   // proper motion
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));   // V magnitude
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));   // colour
        for (GLuint i = 0; i < 4; ++i) glEnableVertexAttribArray(i);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        count = n;
    }

    ~StarField() {
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    }

    StarField(const StarField&)            = delete;
    StarField& operator=(const StarField&) = delete;

    void draw(const glm::mat4& view, const glm::mat4& projection, const glm::mat3& icrfToWorld,
              float yearsSinceJ2000) const {
        if (count == 0) return;
        shader->use();
        glUniformMatrix4fv(locView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix3fv(locIcrf, 1, GL_FALSE, &icrfToWorld[0][0]);
        glUniform1f(locYears, yearsSinceJ2000);
        glDepthMask(GL_FALSE);
        glEnable(GL_PROGRAM_POINT_SIZE);
        glBlendFunc(GL_ONE, GL_ONE);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, count);
        glBindVertexArray(0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_PROGRAM_POINT_SIZE);
        glDepthMask(GL_TRUE);
    }
};
