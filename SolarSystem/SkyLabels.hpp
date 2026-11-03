#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "Hud.hpp"
#include "Shader.hpp"

// The names of the brighter stars, the constellation figures and their names
// (tools/sky_labels.py), each shown or hidden on its own.
// shortcut: names and figures sit at J2000 without proper motion, off by at most
// Arcturus' 2.3"/yr (4' by 2100); move them with the stars if labels must stay on them.
class SkyLabels {
    struct Label { glm::vec3 dir; float alpha; std::string name; };
    std::vector<Label> starNames, constellationNames;
    GLuint VAO = 0, VBO = 0;
    GLsizei vertexCount = 0;
    std::unique_ptr<Shader> shader;
    GLint locView = -1, locProjection = -1, locIcrf = -1, locColor = -1;

    static glm::vec3 direction(float raDeg, float decDeg) {
        const float ra = glm::radians(raDeg), dec = glm::radians(decDeg);
        return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
    }

public:
    bool showStarNames = true, showConstellations = true;

    explicit SkyLabels(const char* path) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "[sky labels] " << path << " missing: no star names or constellations" << std::endl;
            return;
        }
        std::vector<glm::vec3> segments;
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream s(line);
            char kind = 0;
            float ra = 0, dec = 0, a = 0, b = 0;
            s >> kind >> ra >> dec;
            if (kind == 'L' && s >> a >> b) {
                segments.push_back(direction(ra, dec));
                segments.push_back(direction(a, b));
            } else if (kind == 'S' && s >> a) {
                std::string name;
                std::getline(s >> std::ws, name);
                starNames.push_back({direction(ra, dec), std::clamp(0.85f - 0.12f * a, 0.3f, 0.85f), name});
            } else if (kind == 'C') {
                std::string name;
                std::getline(s >> std::ws, name);
                constellationNames.push_back({direction(ra, dec), 0.55f, name});
            }
        }
        try {
            shader = std::make_unique<Shader>("SolarSystem/sky_lines.vert", "SolarSystem/sky_lines.frag");
        } catch (const std::exception& e) {
            std::cerr << "[sky labels] " << e.what() << std::endl;
            return;
        }
        locView = shader->uniformLocation("view");
        locProjection = shader->uniformLocation("projection");
        locIcrf = shader->uniformLocation("icrfToWorld");
        locColor = shader->uniformLocation("color");

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(segments.size() * sizeof(glm::vec3)), segments.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        vertexCount = GLsizei(segments.size());
    }

    ~SkyLabels() {
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
    }

    SkyLabels(const SkyLabels&)            = delete;
    SkyLabels& operator=(const SkyLabels&) = delete;

    // The figures, right after the stars so every body is drawn over them.
    void drawLines(const glm::mat4& view, const glm::mat4& projection, const glm::mat3& icrfToWorld) const {
        if (!showConstellations || vertexCount == 0) return;
        shader->use();
        glUniformMatrix4fv(locView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, &projection[0][0]);
        glUniformMatrix3fv(locIcrf, 1, GL_FALSE, &icrfToWorld[0][0]);
        glUniform4f(locColor, 0.20f, 0.32f, 0.55f, 0.45f);
        glDepthMask(GL_FALSE);
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, vertexCount);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }

    // Screen positions of the names in view; hidden(dir) says a body covers that direction.
    template <typename Hidden>
    void collect(const glm::mat4& view, const glm::mat4& projection, const glm::mat3& icrfToWorld,
                 unsigned int width, unsigned int height, Hidden hidden, std::vector<BodyMarker>& out) const {
        out.clear();
        const auto add = [&](const std::vector<Label>& labels, glm::vec3 color, float radius) {
            for (const Label& l : labels) {
                const glm::vec3 dir = icrfToWorld * l.dir;
                const glm::vec4 clip = projection * glm::vec4(glm::mat3(view) * dir, 1.0f);
                if (clip.w <= 0.0f) continue;
                const glm::vec2 ndc = glm::vec2(clip) / clip.w;
                if (std::abs(ndc.x) > 1.0f || std::abs(ndc.y) > 1.0f || hidden(dir)) continue;
                BodyMarker m;
                m.name = l.name;
                m.x = (ndc.x * 0.5f + 0.5f) * float(width);
                m.y = (0.5f - ndc.y * 0.5f) * float(height);
                m.radius = radius;
                m.labelAlpha = l.alpha;
                m.color = color;
                out.push_back(std::move(m));
            }
        };
        // Constellations first: they win the overlap test against star names.
        if (showConstellations) add(constellationNames, {0.45f, 0.60f, 0.85f}, 0.0f);   // radius 0: centred
        if (showStarNames) add(starNames, {0.80f, 0.82f, 0.88f}, 3.0f);
    }
};
