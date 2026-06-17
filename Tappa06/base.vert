#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;

uniform mat4 modello;    
uniform mat4 vista;      
uniform mat4 proiezione; 

out vec3 vertexColor;

void main() {
    gl_Position = proiezione * vista * modello * vec4(aPos, 1.0);
    vertexColor = aColor; 
}