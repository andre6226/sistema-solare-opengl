#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>
#include "Geometry.hpp"

// UV sphere: `parallels` subdivisions along latitude, `meridians` along longitude.
class Sphere : public Geometry {
private:
    unsigned int VAO, VBO, EBO;
    int indexCount;

public:
    Sphere(float radius, int parallels, int meridians) {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        for (int i = 0; i <= parallels; ++i) {
            float phi = glm::pi<float>() * (float)i / parallels;
            for (int j = 0; j <= meridians; ++j) {
                float theta = 2.0f * glm::pi<float>() * (float)j / meridians;

                float x = radius * std::sin(phi) * std::cos(theta);
                float y = radius * std::cos(phi);
                float z = radius * std::sin(phi) * std::sin(theta);

                // --- UV COORDINATES ---
                // U: fraction of the horizontal turn completed (FLIPPED, because
                // increasing theta moves to the observer's left when seen from
                // outside, which would mirror the texture).
                float u = 1.0f - ((float)j / meridians);
                // V: fraction of the vertical descent (flipped for OpenGL, whose
                // texture origin is at the bottom).
                float v = 1.0f - ((float)i / parallels);

                // Eight floats per vertex
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);
                vertices.push_back(x / radius); // Normal X
                vertices.push_back(y / radius); // Normal Y
                vertices.push_back(z / radius); // Normal Z
                vertices.push_back(u);          // UV: U coordinate
                vertices.push_back(v);          // UV: V coordinate
            }
        }

        for (int i = 0; i < parallels; ++i) {
            for (int j = 0; j < meridians; ++j) {
                int p1 = i * (meridians + 1) + j;
                int p2 = p1 + meridians + 1;

                // Counter-clockwise winding as seen from OUTSIDE the sphere, so
                // that the default glFrontFace(GL_CCW) treats the outer surface
                // as the front face. The original order was the mirror of this
                // and only worked because face culling was disabled.
                indices.push_back(p1);
                indices.push_back(p1 + 1);
                indices.push_back(p2);

                indices.push_back(p1 + 1);
                indices.push_back(p2 + 1);
                indices.push_back(p2);
            }
        }
        indexCount = static_cast<int>(indices.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // --- VERTEX ATTRIBUTES (stride of 8 floats) ---

        // Position (location = 0) - size 3
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Normal (location = 1) - size 3 - offset 3
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        // Texture UV (location = 2) - size 2 - offset 6
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0); // Unbind the VAO
    }

    // Copy and move are already forbidden by Geometry: all that is left here is
    // handing the three buffers back to the driver.
    ~Sphere() override {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void draw() override {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};
