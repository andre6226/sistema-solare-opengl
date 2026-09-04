#pragma once

// Common interface for primitives that own OpenGL resources (VAO/VBO/EBO).
//
// Copying and moving are forbidden at the base level: OpenGL handles are unique
// identifiers, so a copy would make the duplicate call glDelete* on resources
// still in use by the original (a double free on the driver side). Deleting
// them here means every derived class inherits the restriction for free.
class Geometry {
public:
    Geometry() = default;
    virtual ~Geometry() = default;

    Geometry(const Geometry&)            = delete;
    Geometry& operator=(const Geometry&) = delete;
    Geometry(Geometry&&)                 = delete;
    Geometry& operator=(Geometry&&)      = delete;

    virtual void draw() = 0;
};
