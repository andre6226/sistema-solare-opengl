#version 410 core
in vec3 vNormal;
in vec3 vPosizioneMondo;

out vec4 FragColor;

uniform vec3 coloreOggetto;
uniform vec3 cameraPos; // Nuova uniform
uniform bool isSole;

void main() {
    if (isSole) {
        FragColor = vec4(coloreOggetto, 1.0);
        return;
    }

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.0, 0.0, 0.0) - vPosizioneMondo); // Il sole è in (0,0,0)
    vec3 viewDir = normalize(cameraPos - vPosizioneMondo);
    
    // --- Lambert (Diffusa) ---
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0); 

    // --- Blinn-Phong (Speculare) ---
    vec3 halfwayDir = normalize(lightDir + viewDir); // Blinn-Phong
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 8.0);
    vec3 specular = spec * vec3(1.0, 1.0, 1.0); // Riflesso bianco

    vec3 ambient = vec3(0.05, 0.05, 0.05);
    
    FragColor = vec4((ambient + diffuse + specular) * coloreOggetto, 1.0);
}