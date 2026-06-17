#version 410 core
in vec3 vNormal;
in vec3 vPosizioneMondo;

out vec4 FragColor;

uniform vec3 coloreOggetto;
uniform bool isSole; 
void main() {
    if (isSole) {
        FragColor = vec4(coloreOggetto, 1.0);
        return;
    }

    vec3 posizioneSole = vec3(0.0, 0.0, 0.0);
    vec3 direzioneLuce = normalize(posizioneSole - vPosizioneMondo);
    
    float diff = max(dot(normalize(vNormal), direzioneLuce), 0.0);
    
    vec3 ambient = vec3(0.05, 0.05, 0.05);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0); // luce bianca dal Sole
    
    FragColor = vec4((ambient + diffuse) * coloreOggetto, 1.0);
}