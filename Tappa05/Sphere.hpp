#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

class Sphere {
private:
    unsigned int VAO, VBO, EBO;
    int indexCount;

public:
    Sphere(float radius, int paralleliCount, int meridianiCount) {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        for(int i = 0; i <= paralleliCount; ++i) {
            float phi = glm::pi<float>() * (float)i / paralleliCount; 
            for(int j = 0; j <= meridianiCount; ++j) {
                float theta = 2.0f * glm::pi<float>() * (float)j / meridianiCount;

                float x = radius * std::sin(phi) * std::cos(theta);
                float y = radius * std::cos(phi);
                float z = radius * std::sin(phi) * std::sin(theta);

                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                vertices.push_back(std::abs(x) / radius);
                vertices.push_back(std::abs(y) / radius);
                vertices.push_back(std::abs(z) / radius);
            }
        }

        for(int i = 0; i < paralleliCount; ++i) {
            for(int j = 0; j < meridianiCount; ++j) {
                int p1 = i * (meridianiCount + 1) + j;
                int p2 = p1 + meridianiCount + 1;

                indices.push_back(p1);
                indices.push_back(p2);
                indices.push_back(p1 + 1);

                indices.push_back(p1 + 1);
                indices.push_back(p2);
                indices.push_back(p2 + 1);
            }
        }
        indexCount = indices.size();

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Posizione (Location = 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // Colore (Location = 1)
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0); // Stacco il VAO
    }

    void draw() {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};