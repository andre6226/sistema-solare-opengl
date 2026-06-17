#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include "Sphere.hpp"
#include "Shader.hpp"

class CorpoCeleste {
private:
    Sphere* geometria;
    float raggioOrbita;
    float velocitaOrbitale;
    float velocitaRotazioneAssiale; 
    float scala;
    float angoloOrbita;
    float angoloRotazione;      
    
    CorpoCeleste* padre;
    std::vector<CorpoCeleste*> lune;

public:
    CorpoCeleste(Sphere* geo, float raggio, float velOrbita, float velRotazione, float dimensione)
        : geometria(geo), raggioOrbita(raggio), velocitaOrbitale(velOrbita), 
          velocitaRotazioneAssiale(velRotazione), scala(dimensione), 
          angoloOrbita(0.0f), angoloRotazione(0.0f), padre(nullptr) {}

    void aggiungiSatellite(CorpoCeleste* satellite) {
        satellite->padre = this;
        lune.push_back(satellite);
    }

    void aggiorna(float deltaTempo) {
        angoloOrbita += velocitaOrbitale * deltaTempo;
        if (angoloOrbita > glm::two_pi<float>()) angoloOrbita -= glm::two_pi<float>();

        angoloRotazione += velocitaRotazioneAssiale * deltaTempo;
        if (angoloRotazione > glm::two_pi<float>()) angoloRotazione -= glm::two_pi<float>();

        for (CorpoCeleste* luna : lune) luna->aggiorna(deltaTempo);
    }

    void disegna(Shader& shader, glm::mat4 matricePadre) {
        glm::mat4 matriceOrbitale = glm::mat4(1.0f);
        matriceOrbitale = glm::rotate(matriceOrbitale, angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceOrbitale = glm::translate(matriceOrbitale, glm::vec3(raggioOrbita, 0.0f, 0.0f));
        
        glm::mat4 matriceGlobale = matricePadre * matriceOrbitale;

        // PRIMA la rotazione 
        glm::mat4 matriceDisegno = glm::rotate(matriceGlobale, angoloRotazione, glm::vec3(0.0f, 1.0f, 0.0f));
        // e poi la scala
        matriceDisegno = glm::scale(matriceDisegno, glm::vec3(scala));

        // Invio allo shader e disegno
        GLint modelloLoc = glGetUniformLocation(shader.ID, "modello");
        glUniformMatrix4fv(modelloLoc, 1, GL_FALSE, &matriceDisegno[0][0]);
        geometria->draw();

        for (CorpoCeleste* luna : lune) {
            luna->disegna(shader, matriceGlobale);
        }
    }
};