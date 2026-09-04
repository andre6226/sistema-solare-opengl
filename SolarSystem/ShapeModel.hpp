#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "Geometry.hpp"

// A triangle mesh loaded from a measured shape model.
//
// Small bodies are not spheres, and the shapes of Phobos, Deimos and of every
// asteroid ever visited are published as plate models: a list of vertices plus
// a list of triangular facets, derived from spacecraft imagery. Two container
// formats cover essentially everything distributed by the PDS Small Bodies Node
// and by NASA's 3D resources:
//
//   .obj  Wavefront:  "v x y z" lines, then "f a b c" lines (1-based indices)
//   .tab  PDS plate:   a "<vertices> <facets>" header, then one line per vertex
//                      and one per facet, each optionally prefixed by its index
//   .tab  PDS grid:    "latitude longitude radius" rows on a regular lat/lon
//                      grid, which is how the Thomas models of Phobos, Deimos,
//                      Gaspra, Ida, Mathilde and Vesta are published
//
// The two .tab layouts are told apart by how many numbers their first row
// carries, so the caller never has to say which is which. The loader is
// format-driven rather than body-specific, so the same class serves an asteroid
// belt without changes.
class ShapeModel : public Geometry {
public:
    explicit ShapeModel(const std::string& path) {
        std::vector<glm::vec3> positions;
        std::vector<glm::uvec3> faces;

        if (endsWith(path, ".obj")) parseWavefront(path, positions, faces);
        else                        parseTabular(path, positions, faces);

        if (positions.empty() || faces.empty()) {
            throw std::runtime_error("shape model '" + path + "' contains no geometry");
        }

        centreAndNormalise(positions);
        ensureOutwardWinding(positions, faces);

        std::vector<glm::vec3> normals = computeNormals(positions, faces);
        std::vector<glm::vec2> uvs = computeSphericalUVs(positions);
        splitSeam(positions, normals, uvs, faces);

        upload(positions, normals, uvs, faces);

        vertexCount = positions.size();
        faceCount   = faces.size();
    }

    ~ShapeModel() override {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void draw() override {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    std::size_t getVertexCount() const { return vertexCount; }
    std::size_t getFaceCount() const { return faceCount; }

private:
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;
    std::size_t vertexCount = 0;
    std::size_t faceCount = 0;

    static bool endsWith(const std::string& text, const std::string& suffix) {
        if (suffix.size() > text.size()) return false;
        return std::equal(suffix.rbegin(), suffix.rend(), text.rbegin(),
                          [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    }

    static std::ifstream openOrThrow(const std::string& path) {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("could not open shape model '" + path + "'");
        return file;
    }

    // --- Wavefront OBJ ---
    static void parseWavefront(const std::string& path,
                               std::vector<glm::vec3>& positions,
                               std::vector<glm::uvec3>& faces) {
        std::ifstream file = openOrThrow(path);
        std::string line;

        while (std::getline(file, line)) {
            std::istringstream stream(line);
            std::string tag;
            stream >> tag;

            if (tag == "v") {
                glm::vec3 p;
                stream >> p.x >> p.y >> p.z;
                positions.push_back(p);
            } else if (tag == "f") {
                // Faces may be polygons and may carry texture/normal indices as
                // "a/b/c"; only the position index matters here.
                std::vector<unsigned int> corners;
                std::string token;
                while (stream >> token) {
                    const std::size_t slash = token.find('/');
                    if (slash != std::string::npos) token = token.substr(0, slash);
                    if (token.empty()) continue;
                    corners.push_back(static_cast<unsigned int>(std::stoul(token)) - 1u);
                }
                // Fan-triangulate anything with more than three corners.
                for (std::size_t i = 2; i < corners.size(); ++i) {
                    faces.push_back({corners[0], corners[i - 1], corners[i]});
                }
            }
        }
    }

    // Both PDS layouts live in .tab files, so the first row decides: two numbers
    // are a "<vertices> <facets>" plate header, three are a grid sample.
    static void parseTabular(const std::string& path,
                             std::vector<glm::vec3>& positions,
                             std::vector<glm::uvec3>& faces) {
        std::ifstream probe = openOrThrow(path);
        std::string line;
        std::size_t columns = 0;

        while (std::getline(probe, line)) {
            std::istringstream stream(line);
            double value = 0.0;
            columns = 0;
            while (stream >> value) ++columns;
            if (columns > 0) break;
        }

        if (columns == 3) parseRadiusGrid(path, positions, faces);
        else              parsePlateModel(path, positions, faces);
    }

    // --- PDS lat/lon/radius grid ---
    // The rows are ordered latitude-major, and longitude covers 0 to 360
    // inclusive, so the first and last column coincide exactly the way the UV
    // sphere's seam columns do. That makes the grid directly convertible into
    // the same topology the rest of the project already uses.
    static void parseRadiusGrid(const std::string& path,
                                std::vector<glm::vec3>& positions,
                                std::vector<glm::uvec3>& faces) {
        std::ifstream file = openOrThrow(path);

        std::vector<glm::dvec3> samples; // latitude, longitude, radius
        std::string line;

        while (std::getline(file, line)) {
            std::istringstream stream(line);
            double latitude = 0.0, longitude = 0.0, radius = 0.0;
            if (stream >> latitude >> longitude >> radius) {
                samples.push_back(glm::dvec3(latitude, longitude, radius));
            }
        }

        if (samples.size() < 4) {
            throw std::runtime_error("shape model '" + path + "' has too few grid samples");
        }

        // The first run of equal latitudes gives the width of the grid.
        std::size_t longitudeCount = 1;
        while (longitudeCount < samples.size() &&
               samples[longitudeCount].x == samples[0].x) {
            ++longitudeCount;
        }

        const std::size_t latitudeCount = samples.size() / longitudeCount;
        if (longitudeCount < 3 || latitudeCount < 3) {
            throw std::runtime_error("shape model '" + path + "' is not a usable lat/lon grid");
        }

        positions.reserve(latitudeCount * longitudeCount);
        for (std::size_t k = 0; k < latitudeCount * longitudeCount; ++k) {
            // Latitude is measured from the equator, the polar angle from the
            // north pole, matching the convention Sphere generates.
            const double phi   = glm::radians(90.0 - samples[k].x);
            const double theta = glm::radians(samples[k].y);
            const double r     = samples[k].z;

            positions.push_back(glm::vec3(r * std::sin(phi) * std::cos(theta),
                                          r * std::cos(phi),
                                          r * std::sin(phi) * std::sin(theta)));
        }

        // The grid runs south to north, the opposite of Sphere, so these facets
        // come out wound the other way. ensureOutwardWinding() detects and fixes
        // that rather than the orientation being assumed here.
        for (std::size_t i = 0; i + 1 < latitudeCount; ++i) {
            for (std::size_t j = 0; j + 1 < longitudeCount; ++j) {
                const unsigned int p1 = static_cast<unsigned int>(i * longitudeCount + j);
                const unsigned int p2 = static_cast<unsigned int>(p1 + longitudeCount);

                faces.push_back({p1, p1 + 1u, p2});
                faces.push_back({p1 + 1u, p2 + 1u, p2});
            }
        }
    }

    // --- PDS plate model ---
    static void parsePlateModel(const std::string& path,
                                std::vector<glm::vec3>& positions,
                                std::vector<glm::uvec3>& faces) {
        std::ifstream file = openOrThrow(path);

        std::size_t vertexTotal = 0, facetTotal = 0;
        file >> vertexTotal >> facetTotal;
        if (!file || vertexTotal == 0 || facetTotal == 0) {
            throw std::runtime_error("shape model '" + path + "' has no valid plate header");
        }

        positions.reserve(vertexTotal);
        faces.reserve(facetTotal);

        std::string line;
        std::getline(file, line); // finish the header line

        // Some releases prefix every row with its own index and some do not, so
        // rows are classified by how many numbers they carry.
        const auto readRow = [&file, &line](double& a, double& b, double& c) {
            while (std::getline(file, line)) {
                std::istringstream stream(line);
                std::vector<double> values;
                double value = 0.0;
                while (stream >> value) values.push_back(value);

                if (values.size() == 3) { a = values[0]; b = values[1]; c = values[2]; return true; }
                if (values.size() >= 4) { a = values[1]; b = values[2]; c = values[3]; return true; }
            }
            return false;
        };

        for (std::size_t i = 0; i < vertexTotal; ++i) {
            double x = 0, y = 0, z = 0;
            if (!readRow(x, y, z)) break;
            positions.push_back(glm::vec3(x, y, z));
        }

        for (std::size_t i = 0; i < facetTotal; ++i) {
            double a = 0, b = 0, c = 0;
            if (!readRow(a, b, c)) break;
            faces.push_back({static_cast<unsigned int>(a) - 1u,
                             static_cast<unsigned int>(b) - 1u,
                             static_cast<unsigned int>(c) - 1u});
        }
    }

    // Centres the model on its bounding centre and scales it so the farthest
    // vertex sits at radius 1. CelestialBody then applies the render scale, and
    // getBoundingRadius() stays the true extent, which is what the camera's
    // minimum zoom relies on.
    static void centreAndNormalise(std::vector<glm::vec3>& positions) {
        glm::vec3 low(positions[0]), high(positions[0]);
        for (const glm::vec3& p : positions) {
            low = glm::min(low, p);
            high = glm::max(high, p);
        }
        const glm::vec3 centre = (low + high) * 0.5f;

        float maxRadius = 0.0f;
        for (glm::vec3& p : positions) {
            p -= centre;
            maxRadius = std::max(maxRadius, glm::length(p));
        }
        if (maxRadius <= 0.0f) return;

        for (glm::vec3& p : positions) p /= maxRadius;
    }

    // Back-face culling is enabled globally, so a model authored with the
    // opposite convention would render inside-out. Rather than trust the file,
    // compare each facet's normal with its outward direction and flip the whole
    // mesh if the majority disagree.
    static void ensureOutwardWinding(const std::vector<glm::vec3>& positions,
                                     std::vector<glm::uvec3>& faces) {
        long long outward = 0, inward = 0;

        for (const glm::uvec3& f : faces) {
            const glm::vec3& a = positions[f.x];
            const glm::vec3& b = positions[f.y];
            const glm::vec3& c = positions[f.z];

            const glm::vec3 normal = glm::cross(b - a, c - a);
            const glm::vec3 centroid = (a + b + c) / 3.0f;

            if (glm::dot(normal, centroid) >= 0.0f) ++outward;
            else                                    ++inward;
        }

        if (inward > outward) {
            for (glm::uvec3& f : faces) std::swap(f.y, f.z);
        }
    }

    // Area-weighted vertex normals: the cross product is left unnormalised so
    // that larger facets contribute proportionally.
    static std::vector<glm::vec3> computeNormals(const std::vector<glm::vec3>& positions,
                                                 const std::vector<glm::uvec3>& faces) {
        std::vector<glm::vec3> normals(positions.size(), glm::vec3(0.0f));

        for (const glm::uvec3& f : faces) {
            const glm::vec3 normal = glm::cross(positions[f.y] - positions[f.x],
                                                positions[f.z] - positions[f.x]);
            normals[f.x] += normal;
            normals[f.y] += normal;
            normals[f.z] += normal;
        }

        for (std::size_t i = 0; i < normals.size(); ++i) {
            normals[i] = (glm::length(normals[i]) > 0.0f) ? glm::normalize(normals[i])
                                                          : glm::normalize(positions[i]);
        }
        return normals;
    }

    // Equirectangular mapping matching the convention used by Sphere, so the
    // existing body textures line up the same way on either geometry.
    static std::vector<glm::vec2> computeSphericalUVs(const std::vector<glm::vec3>& positions) {
        std::vector<glm::vec2> uvs(positions.size());

        for (std::size_t i = 0; i < positions.size(); ++i) {
            const glm::vec3 d = glm::normalize(positions[i]);

            float theta = std::atan2(d.z, d.x);
            if (theta < 0.0f) theta += glm::two_pi<float>();

            uvs[i] = glm::vec2(1.0f - theta / glm::two_pi<float>(),
                               1.0f - std::acos(glm::clamp(d.y, -1.0f, 1.0f)) / glm::pi<float>());
        }
        return uvs;
    }

    // A shared-vertex mesh has no seam, but its U coordinate does: the facets
    // straddling the 1 -> 0 wrap would run backwards across the whole texture.
    // Those facets get their own copies of the low-U vertices, shifted past 1,
    // where GL_REPEAT samples exactly the right texels.
    static void splitSeam(std::vector<glm::vec3>& positions,
                          std::vector<glm::vec3>& normals,
                          std::vector<glm::vec2>& uvs,
                          std::vector<glm::uvec3>& faces) {
        std::unordered_map<unsigned int, unsigned int> duplicated;
        const std::size_t originalCount = positions.size();

        const auto wrappedCopy = [&](unsigned int index) {
            const auto found = duplicated.find(index);
            if (found != duplicated.end()) return found->second;

            const unsigned int copy = static_cast<unsigned int>(positions.size());
            positions.push_back(positions[index]);
            normals.push_back(normals[index]);
            uvs.push_back(glm::vec2(uvs[index].x + 1.0f, uvs[index].y));
            duplicated.emplace(index, copy);
            return copy;
        };

        for (glm::uvec3& f : faces) {
            const float u0 = uvs[f.x].x, u1 = uvs[f.y].x, u2 = uvs[f.z].x;
            const float span = std::max({u0, u1, u2}) - std::min({u0, u1, u2});
            if (span <= 0.5f) continue;

            unsigned int* corner[3] = {&f.x, &f.y, &f.z};
            for (unsigned int* index : corner) {
                if (*index < originalCount && uvs[*index].x < 0.5f) {
                    *index = wrappedCopy(*index);
                }
            }
        }
    }

    void upload(const std::vector<glm::vec3>& positions,
                const std::vector<glm::vec3>& normals,
                const std::vector<glm::vec2>& uvs,
                const std::vector<glm::uvec3>& faces) {
        std::vector<float> vertices;
        vertices.reserve(positions.size() * 8);

        for (std::size_t i = 0; i < positions.size(); ++i) {
            vertices.insert(vertices.end(), {positions[i].x, positions[i].y, positions[i].z,
                                             normals[i].x, normals[i].y, normals[i].z,
                                             uvs[i].x, uvs[i].y});
        }

        std::vector<unsigned int> indices;
        indices.reserve(faces.size() * 3);
        for (const glm::uvec3& f : faces) {
            indices.insert(indices.end(), {f.x, f.y, f.z});
        }
        indexCount = static_cast<GLsizei>(indices.size());

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        const GLsizei stride = 8 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }
};
