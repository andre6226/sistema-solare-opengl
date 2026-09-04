#pragma once
#include <algorithm>
#include <vector>
#include <memory>
#include "CorpoCeleste.hpp"
#include "Sphere.hpp"
#include "Quad.hpp"
#include "DatiAstronomici.hpp"

class SistemaSolare {
private:
    // L'ORDINE DI DICHIARAZIONE E' SIGNIFICATIVO: i membri vengono distrutti in
    // ordine inverso, quindi le geometrie e le texture devono stare PRIMA dei
    // corpi che le puntano, altrimenti durante la distruzione dei CorpoCeleste
    // i loro Texture*/Geometria* sarebbero gia' penzolanti.
    Sphere geometriaBase;
    Quad geometriaAnelli;
    Astr::Convertitore convertitore;
    std::vector<std::unique_ptr<Texture>> texturesCaricate;

    std::vector<std::unique_ptr<CorpoCeleste>> corpi;
    std::unique_ptr<CorpoCeleste> stelleSfondo;

    CorpoCeleste* sole;
    CorpoCeleste* bersaglioAttuale;

public:
    SistemaSolare() : geometriaBase(1.0f, 100, 50), sole(nullptr), bersaglioAttuale(nullptr) {
        costruisciSistema();
        bersaglioAttuale = sole;
    }

    // Contiene geometrie con handle OpenGL e una rete di puntatori interni:
    // copiarlo o spostarlo non avrebbe senso e romperebbe l'albero.
    SistemaSolare(const SistemaSolare&)            = delete;
    SistemaSolare& operator=(const SistemaSolare&) = delete;
    SistemaSolare(SistemaSolare&&)                 = delete;
    SistemaSolare& operator=(SistemaSolare&&)      = delete;

    void selezionaPadre() {
        if (bersaglioAttuale && bersaglioAttuale->getPadre() != nullptr) {
            bersaglioAttuale = bersaglioAttuale->getPadre();
        }
    }

    void selezionaPrimoFiglio() {
        if (!bersaglioAttuale) return;

        for (CorpoCeleste* figlio : bersaglioAttuale->getLune()) {
            if (figlio->isNavigabile()) {
                bersaglioAttuale = figlio;
                return;
            }
        }
    }

    void selezionaProssimoFratello()   { spostaTraFratelli(+1); }
    void selezionaFratelloPrecedente() { spostaTraFratelli(-1); }

    glm::vec3 getPosizioneBersaglio() const {
        if (bersaglioAttuale) return bersaglioAttuale->getPosizioneGlobale();
        return glm::vec3(0.0f);
    }

    std::string getNomeBersaglio() const {
        if (bersaglioAttuale) return bersaglioAttuale->getNome();
        return "Sole";
    }

    // Raggio ingombro del bersaglio: la telecamera lo usa per calcolare quanto
    // puo' avvicinarsi senza finire dentro il corpo.
    float getRaggioBersaglio() const {
        if (bersaglioAttuale) return bersaglioAttuale->getRaggioVisivo();
        return 1.0f;
    }

   CorpoCeleste* creaCorpo(const Astr::CorpoReale& datiReali, CorpoCeleste* padre) {

        float raggioOrbita = convertitore.getDistanza(datiReali);
        float scala        = convertitore.getDimensione(datiReali);
        float velOrbita    = convertitore.getVelocitaOrbitale(datiReali);
        float velRotazione = convertitore.getVelocitaRotazione(datiReali);
        float inclinazione = convertitore.getInclinazione(datiReali);

        std::string percorsoTexture = std::string("resources/") + datiReali.nome + ".jpg";

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



        std::string percorsoStelle = "resources/Stelle.jpg";
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


        std::string percorsoAnelli = "resources/AnelliSaturno.png";
        auto texAnelli = std::make_unique<Texture>(percorsoAnelli);
        Texture* ptrTexAnelli = texAnelli.get();
        texturesCaricate.push_back(std::move(texAnelli));

        auto anelli = std::make_unique<CorpoCeleste>("AnelliSaturno", ptrTexAnelli, &geometriaAnelli,
                                                     0.0f, 0.0f, 0.0f, 0.0f,
                                                     2.3f * convertitore.getDimensione(Astr::Saturno));
        // Gli anelli sono un figlio di Saturno per ereditarne l'inclinazione,
        // ma non sono un bersaglio selezionabile con le frecce.
        anelli->setNavigabile(false);
        saturno->aggiungiSatellite(anelli.get());
        corpi.push_back(std::move(anelli));


        // --- SATELLITI ESTERNI ---
        creaCorpo(Astr::Titania, urano);
        creaCorpo(Astr::Tritone, nettuno);
    }

    void aggiorna(float deltaTempo) {
        if (sole) sole->aggiorna(deltaTempo, glm::mat4(1.0f));
        if (stelleSfondo) stelleSfondo->aggiorna(deltaTempo, glm::mat4(1.0f));
    }

    void disegna(const UniformLocations& locs) const {
        if (sole) {
            sole->disegna(locs);
        }

        if (stelleSfondo) {
            stelleSfondo->disegna(locs);
        }
    }

private:
    // Scorre i fratelli nella direzione data saltando quelli non navigabili.
    // La versione precedente usava un break sul nome "AnelliSaturno": qualsiasi
    // luna aggiunta dopo gli anelli sarebbe diventata irraggiungibile.
    void spostaTraFratelli(int direzione) {
        if (!bersaglioAttuale || !bersaglioAttuale->getPadre()) return;

        const auto& fratelli = bersaglioAttuale->getPadre()->getLune();
        const auto posizione = std::find(fratelli.begin(), fratelli.end(), bersaglioAttuale);
        if (posizione == fratelli.end()) return;

        const int indice = static_cast<int>(std::distance(fratelli.begin(), posizione));
        const int totale = static_cast<int>(fratelli.size());

        for (int i = indice + direzione; i >= 0 && i < totale; i += direzione) {
            if (fratelli[i]->isNavigabile()) {
                bersaglioAttuale = fratelli[i];
                return;
            }
        }
    }
};
