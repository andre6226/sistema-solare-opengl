#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include "Geometria.hpp"
#include "Shader.hpp"
#include "Texture.hpp"

class CorpoCeleste {
private:
    std::string nome;
    Texture* textureCorpo;
    Geometria* geometria;
    float raggioOrbita;
    float velocitaOrbitale;
    float velocitaRotazioneAssiale;
    float inclinazioneAssiale;    
    float scala;
    float angoloOrbita;
    float angoloRotazione;      
    bool inclinaFigli = true;

    glm::vec3 posizioneGlobale;

    CorpoCeleste* padre;
    std::vector<CorpoCeleste*> lune;

public:
    CorpoCeleste(std::string nomeCorpo, Texture* tex, Geometria* geo, float raggio, float velOrbita, float velRotazione, float inclinazione, float dimensione)
            : nome(nomeCorpo), textureCorpo(tex), geometria(geo), raggioOrbita(raggio), velocitaOrbitale(velOrbita), 
            velocitaRotazioneAssiale(velRotazione), inclinazioneAssiale(inclinazione), scala(dimensione), 
            angoloOrbita(0.0f), angoloRotazione(0.0f), padre(nullptr) {}
    
    std::string getNome() const { return nome; }
    
    CorpoCeleste* getPadre() const { return padre; }
    const std::vector<CorpoCeleste*>& getLune() const { return lune; }

    void aggiungiSatellite(CorpoCeleste* satellite) {
        satellite->padre = this;
        lune.push_back(satellite);
    }

    void aggiorna(float deltaTempo, glm::mat4 matricePadre = glm::mat4(1.0f)) {
        angoloOrbita += velocitaOrbitale * deltaTempo;
        if (angoloOrbita > glm::two_pi<float>()) angoloOrbita -= glm::two_pi<float>();

        float velocitaReale = velocitaRotazioneAssiale - velocitaOrbitale;
        angoloRotazione += velocitaReale * deltaTempo;
        
        if (angoloRotazione > glm::two_pi<float>()) angoloRotazione -= glm::two_pi<float>();
        if (angoloRotazione < 0.0f) angoloRotazione += glm::two_pi<float>();

        glm::mat4 matriceOrbitale = glm::mat4(1.0f);
        matriceOrbitale = glm::rotate(matriceOrbitale, angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceOrbitale = glm::translate(matriceOrbitale, glm::vec3(raggioOrbita, 0.0f, 0.0f));
        
        matriceOrbitale = glm::rotate(matriceOrbitale, -angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        
        glm::mat4 matricePosizionePianeta = matricePadre * matriceOrbitale;
        
        posizioneGlobale = glm::vec3(matricePosizionePianeta[3]);

        glm::mat4 matricePerFigli = inclinaFigli ? 
            glm::rotate(matricePosizionePianeta, inclinazioneAssiale, glm::vec3(0.0f, 0.0f, 1.0f)) : 
            matricePosizionePianeta;

        for (CorpoCeleste* luna : lune) {
            luna->aggiorna(deltaTempo, matricePerFigli);
        }
    }

    void disegna(Shader& shader, const UniformLocations& locs, glm::mat4 matricePadre) {            
        glm::mat4 matriceOrbitale = glm::mat4(1.0f);
        matriceOrbitale = glm::rotate(matriceOrbitale, angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceOrbitale = glm::translate(matriceOrbitale, glm::vec3(raggioOrbita, 0.0f, 0.0f));
        //da spazio eliocentrico a spazio galileiano assoluto
        matriceOrbitale = glm::rotate(matriceOrbitale, -angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        
        glm::mat4 matricePosizionePianeta = matricePadre * matriceOrbitale;
        
        posizioneGlobale = glm::vec3(matricePosizionePianeta[3]);

        glm::mat4 matriceGlobaleInclinata = glm::rotate(matricePosizionePianeta, inclinazioneAssiale, glm::vec3(0.0f, 0.0f, 1.0f));
        glm::mat4 matriceDisegno = glm::rotate(matriceGlobaleInclinata, angoloRotazione, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceDisegno = glm::scale(matriceDisegno, glm::vec3(scala));

        glUniformMatrix4fv(locs.modello, 1, GL_FALSE, &matriceDisegno[0][0]);
    
        glm::mat3 normaleMatrice = glm::mat3(glm::transpose(glm::inverse(matriceDisegno)));
        glUniformMatrix3fv(locs.normaleMatrice, 1, GL_FALSE, &normaleMatrice[0][0]);

        glUniform3f(locs.coloreOggetto, 1.0f, 1.0f, 1.0f); 
        glUniform1i(locs.isSole, (nome == "Sole") ? 1 : 0);
        glUniform1i(locs.isAnello, (nome == "AnelliSaturno") ? 1 : 0);
        glUniform1i(locs.isCielo, (nome == "Stelle") ? 1 : 0);

        if (textureCorpo != nullptr) {
            textureCorpo->bind(0); 
        }

        geometria->draw();

        // Passaggio ricorsivo per i figli
        glm::mat4 matricePerFigli = inclinaFigli ? matriceGlobaleInclinata : matricePosizionePianeta;
        for (CorpoCeleste* luna : lune) {
            luna->disegna(shader, locs, matricePerFigli); 
        }
    }

    void setInclinaFigli(bool inclina) {
        inclinaFigli = inclina;
    }

    glm::vec3 getPosizioneGlobale() const {
        return posizioneGlobale;
    }
};