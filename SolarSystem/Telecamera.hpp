#pragma once
#include "../glad/gl.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

class Telecamera {
private:
    float raggio;
    float angoloTheta;
    float angoloPhi;

    // Limiti di zoom: il minimo non e' una costante ma dipende dal corpo
    // inquadrato, altrimenti sul Sole (raggio 3.3) o su Saturno con gli anelli
    // (raggio 2.0) la telecamera finisce dentro l'oggetto.
    float raggioMinimo  = 1.0f;
    float raggioMassimo = 800.0f;

    static constexpr float sensibilita = 0.4f;
    // Zoom moltiplicativo: una tacca di rotellina vale sempre il 10% della
    // distanza attuale, quindi resta usabile sia a 2 che a 800 unita'.
    static constexpr float fattoreZoom = 0.9f;
    // A quante volte il raggio del bersaglio si ferma la telecamera.
    static constexpr float margineBersaglio = 2.5f;

public:
    Telecamera(float distanzaIniziale = 4.0f, float thetaIniziale = 0.0f, float phiIniziale = 20.0f)
        : raggio(distanzaIniziale), angoloTheta(thetaIniziale), angoloPhi(phiIniziale) {}

    void ruota(float deltaX, float deltaY) {

        angoloTheta -= deltaX * sensibilita;
        angoloPhi   += deltaY * sensibilita;

        if (angoloPhi > 89.0f)  angoloPhi = 89.0f;
        if (angoloPhi < -89.0f) angoloPhi = -89.0f;
    }

    // Da chiamare a ogni cambio di bersaglio: ricalcola i limiti di zoom sul
    // raggio ingombro del nuovo corpo e riporta la distanza attuale nel range.
    void adattaAlBersaglio(float raggioBersaglio) {
        raggioMinimo  = std::max(raggioBersaglio * margineBersaglio, 0.01f);
        raggioMassimo = std::max(raggioMinimo * 4.0f, 800.0f);
        raggio        = std::clamp(raggio, raggioMinimo, raggioMassimo);
    }

    void zoom(float quantita) {
        raggio *= std::pow(fattoreZoom, quantita);
        raggio  = std::clamp(raggio, raggioMinimo, raggioMassimo);
    }

    glm::mat4 getMatriceVista(glm::vec3 puntoOsservato) const {
        glm::vec3 posizioneTelecamera = getPosizione(puntoOsservato);

        glm::vec3 vettoreUp(0.0f, 1.0f, 0.0f);

        return glm::lookAt(posizioneTelecamera, puntoOsservato, vettoreUp);
    }

    glm::mat4 getMatriceProiezione(float aspect_ratio) const {
        const float fov = glm::radians(45.0f);

        // Il piano vicino segue la distanza della telecamera. Con il valore
        // fisso di 0.1 le lune piccole (Fobos ha raggio 0.011) venivano tagliate
        // proprio quando ci si avvicinava per osservarle. Il tetto a 0.1 fa si'
        // che alle distanze normali la precisione del depth buffer resti quella
        // di prima.
        const float pianoVicino  = std::clamp(raggio * 0.005f, 0.002f, 0.1f);
        const float pianoLontano = 4000.0f;

        return glm::perspective(fov, aspect_ratio, pianoVicino, pianoLontano);
    }

    glm::vec3 getPosizione(glm::vec3 puntoOsservato) const {
        float thetaRad = glm::radians(angoloTheta);
        float phiRad   = glm::radians(angoloPhi);

        float x = raggio * glm::cos(phiRad) * glm::sin(thetaRad);
        float y = raggio * glm::sin(phiRad);
        float z = raggio * glm::cos(phiRad) * glm::cos(thetaRad);

        return puntoOsservato + glm::vec3(x, y, z);
    }
};
