#pragma once
#include <vector>
#include <memory>
#include "CorpoCeleste.hpp"
#include "Sphere.hpp"
#include "Quad.hpp"
#include "DatiAstronomici.hpp" 

class SistemaSolare {
private:
    Sphere geometriaBase;
    Quad geometriaAnelli; 
    std::vector<std::unique_ptr<CorpoCeleste>> corpi;
    CorpoCeleste* sole; 
    std::unique_ptr<CorpoCeleste> stelleSfondo;
    std::vector<std::unique_ptr<Texture>> texturesCaricate;
    Astr::Convertitore convertitore; 
    CorpoCeleste* bersaglioAttuale;
public:
    SistemaSolare() : geometriaBase(1.0f, 100, 50), sole(nullptr), bersaglioAttuale(nullptr) {
        costruisciSistema();
        bersaglioAttuale = sole; 
    }

    void selezionaPadre() {
        if (bersaglioAttuale && bersaglioAttuale->getPadre() != nullptr) {
            bersaglioAttuale = bersaglioAttuale->getPadre();
        }
    }

    void selezionaPrimoFiglio() {
        if (bersaglioAttuale && !bersaglioAttuale->getLune().empty()) {
            bersaglioAttuale = bersaglioAttuale->getLune().front();
        }
    }

    void selezionaProssimoFratello() {
        if (!bersaglioAttuale || !bersaglioAttuale->getPadre()) return; 
        
        const auto& fratelli = bersaglioAttuale->getPadre()->getLune();
        for (size_t i = 0; i < fratelli.size(); ++i) {
            if (fratelli[i] == bersaglioAttuale) {
                if (i + 1 < fratelli.size()) {
                    if (fratelli[i + 1]->getNome() == "AnelliSaturno") break; 
                    
                    bersaglioAttuale = fratelli[i + 1];
                }
                break;
            }
        }
    }

    void selezionaFratelloPrecedente() {
        if (!bersaglioAttuale || !bersaglioAttuale->getPadre()) return; 
        
        const auto& fratelli = bersaglioAttuale->getPadre()->getLune();
        for (size_t i = 0; i < fratelli.size(); ++i) {
            if (fratelli[i] == bersaglioAttuale) {
                if (i > 0) bersaglioAttuale = fratelli[i - 1];
                break;
            }
        }
    }

    glm::vec3 getPosizioneBersaglio() const {
        if (bersaglioAttuale) return bersaglioAttuale->getPosizioneGlobale();
        return glm::vec3(0.0f);
    }

    std::string getNomeBersaglio() const {
        if (bersaglioAttuale) return bersaglioAttuale->getNome();
        return "Sole";
    }

   CorpoCeleste* creaCorpo(const Astr::CorpoReale& datiReali, CorpoCeleste* padre) {
        
        float raggioOrbita = convertitore.getDistanza(datiReali);
        float scala        = convertitore.getDimensione(datiReali);
        float velOrbita    = convertitore.getVelocitaOrbitale(datiReali);
        float velRotazione = convertitore.getVelocitaRotazione(datiReali);
        float inclinazione = convertitore.getInclinazione(datiReali); // NOVITÀ

        std::string percorsoTexture = std::string("Risorse/") + datiReali.nome + ".jpg";
        
        auto nuovaTexture = std::make_unique<Texture>(percorsoTexture);
        Texture* ptrTexture = nuovaTexture.get();
        texturesCaricate.push_back(std::move(nuovaTexture));

        auto nuovoCorpo = std::make_unique<CorpoCeleste>(datiReali.nome, ptrTexture, &geometriaBase, raggioOrbita, velOrbita, velRotazione, inclinazione, scala);        
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



        std::string percorsoStelle = "Risorse/Stelle.jpg"; 
        auto texStelle = std::make_unique<Texture>(percorsoStelle);
        Texture* ptrTexStelle = texStelle.get();
        texturesCaricate.push_back(std::move(texStelle));
        
        stelleSfondo = std::make_unique<CorpoCeleste>("Stelle", ptrTexStelle, &geometriaBase, 0.0f, 0.0f, 0.0f, 0.0f, 2500.0f);


        // --- PIANETI INTERNI E LUNE ---
        creaCorpo(Astr::Mercurio, sole);
        creaCorpo(Astr::Venere, sole);
        
        CorpoCeleste* terra = creaCorpo(Astr::Terra, sole);
        terra->setInclinaFigli(false); // la Luna segue l'eclittica e non la terra
        creaCorpo(Astr::Luna, terra);

        CorpoCeleste* marte = creaCorpo(Astr::Marte, sole);
        creaCorpo(Astr::Deimos, marte);
        creaCorpo(Astr::Phobos, marte);

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


        // --- ACCROCCHIO ANELLI DI SATURNO ---

        std::string percorsoAnelli = "Risorse/AnelliSaturno.png"; 
        auto texAnelli = std::make_unique<Texture>(percorsoAnelli);
        Texture* ptrTexAnelli = texAnelli.get();
        texturesCaricate.push_back(std::move(texAnelli));

        auto anelli = std::make_unique<CorpoCeleste>("AnelliSaturno", ptrTexAnelli, &geometriaAnelli, 0.0f, 0.0f, 0.0f, 0.0f, 2.3f * convertitore.getDimensione(Astr::Saturno));        saturno->aggiungiSatellite(anelli.get());
        corpi.push_back(std::move(anelli));




        // --- SATELLITI ESTERNI ---
        creaCorpo(Astr::Titania, urano);
        creaCorpo(Astr::Tritone, nettuno);
    }

    void aggiorna(float deltaTempo) {
        if (sole) sole->aggiorna(deltaTempo, glm::mat4(1.0f));
        if (stelleSfondo) stelleSfondo->aggiorna(deltaTempo, glm::mat4(1.0f));
    }

    void disegna(Shader& shader, const UniformLocations& locs) {
        if (sole) {
            glm::mat4 centroDellUniverso = glm::mat4(1.0f);
            sole->disegna(shader, locs, centroDellUniverso);
        }

        if (stelleSfondo) {
            stelleSfondo->disegna(shader, locs, glm::mat4(1.0f)); // <--- AGGIUNTO
        }


    }
};