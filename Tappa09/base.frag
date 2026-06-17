#version 410 core
in vec3 vPosizioneMondo;
in vec3 vNormal;
in vec2 TexCoord; // <--- Ricevute dal Vertex

out vec4 FragColor;

uniform vec3 cameraPos;
uniform bool isSole;
uniform sampler2D texturePianeta; 

void main() {
    vec4 coloreTexture = texture(texturePianeta, TexCoord);

    if (isSole) {
        // Il sole non ha ombre, è luce pura
        FragColor = coloreTexture;
        return;
    }

    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.0, 0.0, 0.0) - vPosizioneMondo);
    vec3 viewDir = normalize(cameraPos - vPosizioneMondo);

    // Lambert (Diffusa)
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Blinn-Phong (Speculare)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * vec3(0.3); 

    vec3 ambient = vec3(0.05);

    FragColor = vec4(ambient + diffuse + specular, 1.0) * coloreTexture;
}