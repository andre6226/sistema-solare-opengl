#version 410 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords; 

uniform mat4 modello;
uniform mat4 vista;
uniform mat4 proiezione;
uniform mat3 normaleMatrice;

out vec3 vPosizioneMondo;
out vec3 vNormal;
out vec2 TexCoord; 

void main() {
    vec4 posMondo = modello * vec4(aPos, 1.0);
    vPosizioneMondo = posMondo.xyz;
    vNormal = normalize(normaleMatrice * aNormal);
    
    TexCoord = aTexCoords; // Le passiamo così come sono
    
    gl_Position = proiezione * vista * posMondo;
}