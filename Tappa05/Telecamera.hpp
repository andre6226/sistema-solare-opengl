#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Telecamera {
private:
    float raggio;          
    float angoloTheta;     
    float angoloPhi;       
    const float sensibilita = 0.4f; 
    const float velocitaZoom = 1.9f; 


public:
    Telecamera(float distanzaIniziale = 4.0f, float thetaIniziale = 0.0f, float phiIniziale = 20.0f)
        : raggio(distanzaIniziale), angoloTheta(thetaIniziale), angoloPhi(phiIniziale) {}

    void ruota(float deltaX, float deltaY) {
        
        angoloTheta -= deltaX * sensibilita; 
        angoloPhi   += deltaY * sensibilita;

        if (angoloPhi > 89.0f)  angoloPhi = 89.0f;
        if (angoloPhi < -89.0f) angoloPhi = -89.0f;
    }

    void zoom(float quantita) {
        raggio -= quantita * velocitaZoom; 

        if (raggio < 1.0f)  raggio = 1.0f;
        if (raggio > 300.0f) raggio = 300.0f;
    }

    glm::mat4 getMatriceVista() const {
        float thetaRad = glm::radians(angoloTheta);
        float phiRad   = glm::radians(angoloPhi);

        // Equazioni parametriche 
        float x = raggio * glm::cos(phiRad) * glm::sin(thetaRad);
        float y = raggio * glm::sin(phiRad);
        float z = raggio * glm::cos(phiRad) * glm::cos(thetaRad);

        glm::vec3 posizioneTelecamera(x, y, z);
        glm::vec3 puntoOsservato(0.0f, 0.0f, 0.0f); // La telecamera punta sempre fissa sull'origine
        glm::vec3 vettoreUp(0.0f, 1.0f, 0.0f);       

        return glm::lookAt(posizioneTelecamera, puntoOsservato, vettoreUp);
    }

    glm::mat4 getMatriceProiezione(float aspect_ratio) const {
        float fov = glm::radians(45.0f); 
        float pianoVicino = 0.1f;        
        float pianoLontano = 300.0f;     

        return glm::perspective(fov, aspect_ratio, pianoVicino, pianoLontano);
    }
};