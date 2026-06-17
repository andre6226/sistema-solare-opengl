#version 410 core
in vec3 vPosizioneMondo;
in vec3 vNormal;
in vec2 TexCoord; 

out vec4 FragColor;

uniform vec3 cameraPos;
uniform bool isSole;
uniform bool isAnello;
uniform bool isCielo; 
uniform sampler2D texturePianeta;
uniform mat4 modello;

void main() {
    vec2 uv = TexCoord;
    if (isAnello) {
        float dist = length(TexCoord - vec2(0.5, 0.5)) * 2.0;
        float raggioInterno = 0.50; 
        float raggioEsterno = 0.95;

        if (dist < raggioInterno || dist > raggioEsterno) {
            discard;
        }

        float u = (dist - raggioInterno) / (raggioEsterno - raggioInterno);
        uv = vec2(u, 0.5); 
    }

    vec4 coloreTexture = texture(texturePianeta, uv);
    if (coloreTexture.a < 0.1 || (isAnello && length(coloreTexture.rgb) < 0.15)) {
        discard;
    }

    // Se è il sole o il cielo, la texture  è senza ombre
    if (isSole || isCielo) {
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

    if (isAnello) {
        vec3 centroSaturno = vec3(modello[3][0], modello[3][1], modello[3][2]);
        float raggioSaturno = length(vec3(modello[0])) / 2.3;
        vec3 dirLuce = normalize(vPosizioneMondo);
        float distanzaProiezione = dot(centroSaturno, dirLuce);
        vec3 puntoPiuVicino = dirLuce * distanzaProiezione;
        float distanzaDalCentro = length(puntoPiuVicino - centroSaturno);

        if (distanzaDalCentro < (raggioSaturno * 0.98) && distanzaProiezione < length(vPosizioneMondo)) {
            ambient = vec3(0.005);       
            diffuse = vec3(0.0);        
            coloreTexture.rgb *= 0.15;   
        } else {
            ambient = vec3(0.8);        
        }
    }

    FragColor = vec4(ambient + diffuse + specular, 1.0) * coloreTexture;
}