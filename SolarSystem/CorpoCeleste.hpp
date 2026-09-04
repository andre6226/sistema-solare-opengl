#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>
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
    bool navigabile   = true;

    // --- Stato derivato ---
    // Calcolato una sola volta per frame da aggiorna() e soltanto letto da
    // disegna(). Prima le stesse matrici venivano ricostruite in entrambi i
    // metodi: costo doppio e, soprattutto, due copie della stessa formula da
    // tenere allineate a mano.
    glm::vec3 posizioneGlobale{0.0f};
    glm::mat4 matriceDisegno{1.0f};
    glm::mat4 matricePerFigli{1.0f};
    glm::mat3 matriceNormale{1.0f};

    CorpoCeleste* padre;
    std::vector<CorpoCeleste*> lune;

    // Riporta l'angolo in [0, 2pi). La versione precedente sottraeva 2pi una
    // volta sola: con un moltiplicatore di tempo alto l'angolo cresceva senza
    // essere mai riportato nell'intervallo, perdendo precisione in float.
    static float normalizzaAngolo(float angolo) {
        angolo = std::fmod(angolo, glm::two_pi<float>());
        if (angolo < 0.0f) angolo += glm::two_pi<float>();
        return angolo;
    }

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

    void aggiorna(float deltaTempo, const glm::mat4& matricePadre = glm::mat4(1.0f)) {
        angoloOrbita    = normalizzaAngolo(angoloOrbita    + velocitaOrbitale         * deltaTempo);
        angoloRotazione = normalizzaAngolo(angoloRotazione + velocitaRotazioneAssiale * deltaTempo);

        glm::mat4 matriceOrbitale = glm::mat4(1.0f);
        matriceOrbitale = glm::rotate(matriceOrbitale, angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceOrbitale = glm::translate(matriceOrbitale, glm::vec3(raggioOrbita, 0.0f, 0.0f));
        // Annulla la rotazione orbitale: il corpo trasla lungo l'orbita ma
        // conserva un orientamento fisso rispetto allo spazio (galileiano).
        matriceOrbitale = glm::rotate(matriceOrbitale, -angoloOrbita, glm::vec3(0.0f, 1.0f, 0.0f));

        const glm::mat4 matricePosizionePianeta = matricePadre * matriceOrbitale;
        posizioneGlobale = glm::vec3(matricePosizionePianeta[3]);

        const glm::mat4 matriceGlobaleInclinata =
            glm::rotate(matricePosizionePianeta, inclinazioneAssiale, glm::vec3(0.0f, 0.0f, 1.0f));

        matriceDisegno = glm::rotate(matriceGlobaleInclinata, angoloRotazione, glm::vec3(0.0f, 1.0f, 0.0f));
        matriceDisegno = glm::scale(matriceDisegno, glm::vec3(scala));

        matriceNormale = glm::mat3(glm::transpose(glm::inverse(matriceDisegno)));

        // I figli ereditano l'inclinazione assiale solo se orbitano sul piano
        // equatoriale del padre (lune di Marte, di Urano, anelli di Saturno).
        // La Luna invece segue l'eclittica, non l'equatore terrestre.
        matricePerFigli = inclinaFigli ? matriceGlobaleInclinata : matricePosizionePianeta;

        for (CorpoCeleste* luna : lune) {
            luna->aggiorna(deltaTempo, matricePerFigli);
        }
    }

    void disegna(const UniformLocations& locs) const {
        glUniformMatrix4fv(locs.modello, 1, GL_FALSE, &matriceDisegno[0][0]);
        glUniformMatrix3fv(locs.normaleMatrice, 1, GL_FALSE, &matriceNormale[0][0]);

        glUniform1i(locs.isSole, (nome == "Sole") ? 1 : 0);
        glUniform1i(locs.isAnello, (nome == "AnelliSaturno") ? 1 : 0);
        glUniform1i(locs.isCielo, (nome == "Stelle") ? 1 : 0);

        if (textureCorpo != nullptr) {
            textureCorpo->bind(0);
        }

        geometria->draw();

        // Passaggio ricorsivo per i figli
        for (const CorpoCeleste* luna : lune) {
            luna->disegna(locs);
        }
    }

    void setInclinaFigli(bool inclina) {
        inclinaFigli = inclina;
    }

    // I corpi non navigabili (gli anelli) restano nell'albero di rendering ma
    // vengono saltati dalla selezione con le frecce.
    void setNavigabile(bool valore) { navigabile = valore; }
    bool isNavigabile() const { return navigabile; }

    // Raggio della sfera che racchiude il corpo piu' gli elementi solidali a
    // esso, cioe' i figli con raggio orbitale nullo (gli anelli di Saturno).
    // Serve alla telecamera per non entrare dentro l'oggetto inquadrato.
    float getRaggioVisivo() const {
        float raggio = scala;
        for (const CorpoCeleste* figlio : lune) {
            if (figlio->raggioOrbita == 0.0f) {
                raggio = std::max(raggio, figlio->scala);
            }
        }
        return raggio;
    }

    glm::vec3 getPosizioneGlobale() const {
        return posizioneGlobale;
    }
};
