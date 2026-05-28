#pragma once
#include <vector>
#include <memory>
#include "CorpoCeleste.hpp"
#include "Sphere.hpp"

class SistemaSolare {
private:
    Sphere geometriaBase; 
    
    std::vector<std::unique_ptr<CorpoCeleste>> corpi;
    
    CorpoCeleste* sole; 

public:
    SistemaSolare() : geometriaBase(1.0f, 100, 50), sole(nullptr) {
        costruisciSistema();
    }

    CorpoCeleste* creaCorpo(float raggio, float velOrbita, float velRot, float scala, CorpoCeleste* padre) {
        auto nuovoCorpo = std::make_unique<CorpoCeleste>(&geometriaBase, raggio, velOrbita, velRot, scala);
        
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
        
        sole = creaCorpo(0.0f, 0.0f, 0.2f, 2.0f, nullptr);

        CorpoCeleste* terra    = creaCorpo(7.0f, 1.0f, 2.0f, 0.5f, sole);
    }

    void aggiorna(float deltaTempo) {
        if (sole) {
            sole->aggiorna(deltaTempo);
        }
    }

    void disegna(Shader& shader) {
        if (sole) {
            glm::mat4 centroDellUniverso = glm::mat4(1.0f);
            sole->disegna(shader, centroDellUniverso);
        }
    }
};