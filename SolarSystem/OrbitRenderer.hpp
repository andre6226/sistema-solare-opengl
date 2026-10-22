#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "Shader.hpp"
#include "CelestialBody.hpp"
#include "AstronomicalData.hpp"
#include "Orbits.hpp"
#include "Texture.hpp"

class OrbitRenderer {
private:
    struct Batch {
        GLint first;
        GLsizei count;
        glm::vec4 color;
    };

    GLuint VAO = 0;
    GLuint VBO = 0;
    std::unique_ptr<Shader> fileShader;
    GLuint fallbackProgram = 0;
    int mode = 0;   // a Mode

    GLint locView = -1;
    GLint locProjection = -1;
    GLint locOrbitColor = -1;

    std::vector<glm::vec3> vertexBuffer;
    std::vector<Batch> batches;

    static constexpr int NUM_SEGMENTS = 180;
    static constexpr size_t MAX_VISIBLE_ORBITS = 32;
    size_t bufferCapacity = MAX_VISIBLE_ORBITS * NUM_SEGMENTS;

    void useProgram() const {
        if (fileShader) {
            fileShader->use();
        } else if (fallbackProgram != 0) {
            glUseProgram(fallbackProgram);
        }
    }

    void initFallbackShader() {
        const char* vertSrc =
            "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "uniform mat4 view;\n"
            "uniform mat4 projection;\n"
            "void main() {\n"
            "    vec4 clipPos = projection * (view * vec4(aPos, 1.0));\n"
            "    if (clipPos.w > 0.0) {\n"
            "        const float Fcoef = 2.0 / log2(3.0e10 + 1.0);\n"
            "        clipPos.z = (log2(max(1e-6, 1.0 + clipPos.w)) * Fcoef - 1.0) * clipPos.w;\n"
            "    }\n"
            "    gl_Position = clipPos;\n"
            "}\n";

        const char* fragSrc =
            "#version 330 core\n"
            "uniform vec4 orbitColor;\n"
            "out vec4 FragColor;\n"
            "void main() {\n"
            "    FragColor = orbitColor;\n"
            "}\n";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vertSrc, nullptr);
        glCompileShader(vs);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fragSrc, nullptr);
        glCompileShader(fs);

        fallbackProgram = glCreateProgram();
        glAttachShader(fallbackProgram, vs);
        glAttachShader(fallbackProgram, fs);
        glLinkProgram(fallbackProgram);

        glDeleteShader(vs);
        glDeleteShader(fs);

        locView       = glGetUniformLocation(fallbackProgram, "view");
        locProjection = glGetUniformLocation(fallbackProgram, "projection");
        locOrbitColor = glGetUniformLocation(fallbackProgram, "orbitColor");
    }

public:
    enum Mode { Planets, All, None };
    OrbitRenderer() {
        try {
            fileShader = std::make_unique<Shader>("SolarSystem/orbit.vert", "SolarSystem/orbit.frag");
            locView       = fileShader->uniformLocation("view");
            locProjection = fileShader->uniformLocation("projection");
            locOrbitColor = fileShader->uniformLocation("orbitColor");
        } catch (const std::exception& e) {
            initFallbackShader();
        }

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, bufferCapacity * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        vertexBuffer.reserve(bufferCapacity);
        batches.reserve(MAX_VISIBLE_ORBITS);
    }

    ~OrbitRenderer() {
        if (VBO != 0) glDeleteBuffers(1, &VBO);
        if (VAO != 0) glDeleteVertexArrays(1, &VAO);
        if (fallbackProgram != 0) glDeleteProgram(fallbackProgram);
    }

    OrbitRenderer(const OrbitRenderer&)            = delete;
    OrbitRenderer& operator=(const OrbitRenderer&) = delete;
    OrbitRenderer(OrbitRenderer&&)                 = delete;
    OrbitRenderer& operator=(OrbitRenderer&&)      = delete;

    // O: planets and moons, then the minor bodies too, then nothing.
    void toggle() { mode = (mode + 1) % 3; }
    void setMode(Mode m) { mode = m; }
    bool isVisible() const { return mode != None; }
    bool showsMinorBodies() const { return mode == All; }

    static bool shouldRenderOrbit(const CelestialBody* body,
                                  const CelestialBody* currentTarget,
                                  const CelestialBody* sun, bool minorBodies) {
        // The focused body's own orbit would run straight through it up close.
        if (!body || !body->isNavigable() || body == sun || body == currentTarget) return false;
        const CelestialBody* parent = body->getParent();
        if (!parent) return false;
        const Astro::BodyData* data = body->getData();
        if (!data || data->orbit.a0 <= 0.0) return false;

        // Rule 1: bodies directly orbiting the Sun: the planets always, the minor bodies on request
        if (parent == sun) {
            return minorBodies || !Orbits::isMinorBody(body->getName(), true);
        }

        // Rule 2: If target is the Sun or null, all moon orbits are hidden
        if (currentTarget == nullptr || currentTarget == sun) {
            return false;
        }

        // Rule 3: If target is the parent of this moon, show all child moons
        if (currentTarget == parent) {
            return true;
        }

        // Rule 4: If target is a moon, show all sibling moons of the same parent
        const CelestialBody* targetParent = currentTarget->getParent();
        if (targetParent != nullptr && targetParent != sun && targetParent == parent) {
            return true;
        }

        return false;
    }

    void update(const std::vector<std::unique_ptr<CelestialBody>>& bodies,
                const CelestialBody* sun,
                const CelestialBody* currentTarget, double viewKm) {
        if (!isVisible()) return;

        vertexBuffer.clear();
        batches.clear();

        const glm::dvec3 origin = currentTarget ? currentTarget->getExactPosition() : glm::dvec3(0.0);
        const double step = glm::two_pi<double>() / static_cast<double>(NUM_SEGMENTS);

        for (const auto& bodyPtr : bodies) {
            const CelestialBody* body = bodyPtr.get();
            if (!shouldRenderOrbit(body, currentTarget, sun, showsMinorBodies())) continue;

            const Astro::KeplerianBasis& basis = body->getOrbitBasis();
            if (basis.a_km <= 0.0) continue;
            const float fade = Orbits::orbitFade(viewKm, basis.a_km);
            if (fade <= 0.0f) continue;

            const CelestialBody* parent = body->getParent();
            const glm::dvec3 parentExact = parent ? parent->getExactPosition() : glm::dvec3(0.0);
            const GLint startIdx = static_cast<GLint>(vertexBuffer.size());

            // Zero-sagitta alignment: vertex 0 is anchored to current body eccentric anomaly E(t)
            for (int k = 0; k < NUM_SEGMENTS; ++k) {
                const double Ek = basis.E_rad + static_cast<double>(k) * step;
                const glm::dvec3 vRel = Astro::evaluateOrbitVertex(basis, Ek);
                const glm::vec3 vRender = glm::vec3((parentExact - origin) + basis.focus_world + vRel);
                vertexBuffer.push_back(vRender);
            }

            glm::vec3 tint(0.7f, 0.7f, 0.8f);
            if (body->getTexture() != nullptr) {
                tint = body->getTexture()->tint;
            }
            batches.push_back({startIdx, NUM_SEGMENTS, glm::vec4(tint, 0.22f * fade)});
        }

        if (!vertexBuffer.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            if (vertexBuffer.size() > bufferCapacity) {
                bufferCapacity = vertexBuffer.size();
                glBufferData(GL_ARRAY_BUFFER, bufferCapacity * sizeof(glm::vec3),
                             vertexBuffer.data(), GL_DYNAMIC_DRAW);
            } else {
                glBufferSubData(GL_ARRAY_BUFFER, 0,
                                vertexBuffer.size() * sizeof(glm::vec3),
                                vertexBuffer.data());
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
    }

    void draw(const glm::mat4& view, const glm::mat4& projection) const {
        if (!isVisible() || batches.empty()) return;

        useProgram();
        glUniformMatrix4fv(locView, 1, GL_FALSE, &view[0][0]);
        glUniformMatrix4fv(locProjection, 1, GL_FALSE, &projection[0][0]);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_LINE_SMOOTH);

        glBindVertexArray(VAO);
        for (const auto& b : batches) {
            glUniform4fv(locOrbitColor, 1, &b.color[0]);
            glDrawArrays(GL_LINE_LOOP, b.first, b.count);
        }
        glBindVertexArray(0);

        glDisable(GL_LINE_SMOOTH);
        glDepthMask(GL_TRUE);
    }

    void render(const std::vector<std::unique_ptr<CelestialBody>>& bodies,
                const CelestialBody* sun,
                const CelestialBody* currentTarget,
                const glm::mat4& view,
                const glm::mat4& projection,
                double viewKm) {
        update(bodies, sun, currentTarget, viewKm);
        draw(view, projection);
    }
};
