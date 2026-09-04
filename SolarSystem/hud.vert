#version 330 core
layout (location = 0) in vec2 aPos;      // pixel coordinates, origin top-left
layout (location = 1) in vec2 aTexCoord; // normalised atlas coordinates
layout (location = 2) in vec4 aColor;

uniform vec2 screenSize;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
    // Pixels to clip space, with Y running downwards so that laying the HUD out
    // from the top-left needs no flipping on the CPU side.
    vec2 ndc = vec2(aPos.x / screenSize.x, 1.0 - aPos.y / screenSize.y) * 2.0 - 1.0;

    vTexCoord = aTexCoord;
    vColor    = aColor;

    gl_Position = vec4(ndc, 0.0, 1.0);
}
