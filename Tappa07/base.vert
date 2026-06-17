#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; 

uniform mat4 modello;
uniform mat4 vista;
uniform mat4 proiezione;
uniform mat3 normaleMatrice; 

out vec3 vNormal;
out vec3 vPosizioneMondo;

void main() {
    vec4 posizioneMondo = modello * vec4(aPos, 1.0);
    vPosizioneMondo = posizioneMondo.xyz;
    
    vNormal = normalize(normaleMatrice * aNormal);
    
    gl_Position = proiezione * vista * posizioneMondo;
}