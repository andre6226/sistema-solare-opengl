#pragma once
#include <vector>
#include <memory>
#include "CorpoCeleste.hpp"
#include "Sphere.hpp"
#include "DatiAstronomici.hpp" // Inclusione del nostro database reale

class SistemaSolare {
private:
    Sphere geometriaBase; 
    std::vector<std::unique_ptr<CorpoCeleste>> corpi;
    CorpoCeleste* sole; 
    
    Astr::Convertitore convertitore; 
    
public:
    SistemaSolare() : geometriaBase(1.0f, 100, 50), sole(nullptr) {
        costruisciSistema();
    }

   CorpoCeleste* creaCorpo(const Astr::CorpoReale& datiReali, CorpoCeleste* padre) {
        
        float raggioOrbita = convertitore.getDistanza(datiReali);
        float scala        = convertitore.getDimensione(datiReali);
        float velOrbita    = convertitore.getVelocitaOrbitale(datiReali);
        float velRotazione = convertitore.getVelocitaRotazione(datiReali);
        float inclinazione = convertitore.getInclinazione(datiReali); // NOVITÀ

        auto nuovoCorpo = std::make_unique<CorpoCeleste>(&geometriaBase, raggioOrbita, velOrbita, velRotazione, inclinazione, scala);
        
        CorpoCeleste* ptr = nuovoCorpo.get(); 
        
        if (padre != nullptr) {
            padre->aggiungiSatellite(ptr);
        } else if (sole == nullptr) {
            sole = ptr;
        }

        corpi.push_back(std::move(nuovoCorpo));
        return ptr;
    }

    void costruisciSistema() {
        // --- IL CENTRO ---
        sole = creaCorpo(Astr::Sole, nullptr);

        // --- PIANETI INTERNI E LUNE ---
        creaCorpo(Astr::Mercurio, sole);
        creaCorpo(Astr::Venere, sole);
        
        CorpoCeleste* terra = creaCorpo(Astr::Terra, sole);
        terra->setInclinaFigli(false); // la Luna non segue l'eclittica
        creaCorpo(Astr::Luna, terra);

        CorpoCeleste* marte = creaCorpo(Astr::Marte, sole);
        creaCorpo(Astr::Phobos, marte);
        creaCorpo(Astr::Deimos, marte);

        // --- GIGANTI GASSOSI ---
        CorpoCeleste* giove   = creaCorpo(Astr::Giove, sole);
        CorpoCeleste* saturno = creaCorpo(Astr::Saturno, sole);
        CorpoCeleste* urano   = creaCorpo(Astr::Urano, sole);
        CorpoCeleste* nettuno = creaCorpo(Astr::Nettuno, sole);

        // --- SATELLITI GALILEIANI DI GIOVE ---
        creaCorpo(Astr::Io, giove);
        creaCorpo(Astr::Europa, giove);
        creaCorpo(Astr::Ganimede, giove);
        creaCorpo(Astr::Callisto, giove);

        // --- SATELLITI DI SATURNO ---
        creaCorpo(Astr::Titano, saturno);
        creaCorpo(Astr::Encelado, saturno);

        // --- SATELLITI ESTERNI ---
        creaCorpo(Astr::Titania, urano);
        creaCorpo(Astr::Tritone, nettuno); 

    }
    void aggiorna(float deltaTempo) {
        if (sole) sole->aggiorna(deltaTempo);
    }

    void disegna(Shader& shader) {
        if (sole) {
            glm::mat4 centroDellUniverso = glm::mat4(1.0f);
            sole->disegna(shader, centroDellUniverso);
        }
    }
};