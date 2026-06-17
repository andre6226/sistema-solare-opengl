#pragma once
#include <cmath>
#include <string>

namespace Astr {

    struct CorpoReale {
        const char* nome;
        double raggioKm;            
        double distanzaDalPadreKm;  
        double periodoOrbitaleGiorni; 
        double periodoRotazioneGiorni; 
        double inclinazioneGradi;  
    };


    inline constexpr CorpoReale Sole     = {"Sole", 696340.0, 0.0, 0.0, 25.38, 7.25};
    inline constexpr CorpoReale Mercurio = {"Mercurio", 2439.7, 57900000.0, 88.0, 58.6, 0.03};
    inline constexpr CorpoReale Venere   = {"Venere", 6051.8, 108200000.0, 224.7, -243.0, 177.36};
    inline constexpr CorpoReale Terra    = {"Terra", 6371.0, 149600000.0, 365.2, 1.0, 23.44};
    inline constexpr CorpoReale Marte    = {"Marte", 3389.5, 227900000.0, 687.0, 1.03, 25.19};
    inline constexpr CorpoReale Luna     = {"Luna", 1737.4, 384400.0, 27.3, 27.3, 6.68};
    inline constexpr CorpoReale Phobos   = {"Phobos", 11.2, 9376.0, 0.31, 0.31, 0.0};
    inline constexpr CorpoReale Deimos   = {"Deimos", 6.2, 23460.0, 1.26, 1.26, 0.0};
    inline constexpr CorpoReale Giove    = {"Giove", 69911.0, 778500000.0, 4331.0, 0.41, 3.13};
    inline constexpr CorpoReale Saturno  = {"Saturno", 58232.0, 1434000000.0, 10747.0, 0.45, 26.73};
    inline constexpr CorpoReale Urano    = {"Urano", 25362.0, 2871000000.0, 30589.0, -0.72, 97.77}; 
    inline constexpr CorpoReale Nettuno  = {"Nettuno", 24622.0, 4495000000.0, 59800.0, 0.67, 28.32};
    inline constexpr CorpoReale Io       = {"Io", 1821.6, 421700.0, 1.76, 1.76, 0.0};
    inline constexpr CorpoReale Europa   = {"Europa", 1560.8, 671100.0, 3.55, 3.55, 0.0};
    inline constexpr CorpoReale Ganimede = {"Ganimede", 2634.1, 1070400.0, 7.15, 7.15, 0.0};
    inline constexpr CorpoReale Callisto = {"Callisto", 2410.3, 1882700.0, 16.68, 16.68, 0.0};
    inline constexpr CorpoReale Titano   = {"Titano", 2574.7, 1221800.0, 15.94, 15.94, 0.0};
    inline constexpr CorpoReale Encelado = {"Encelado", 252.1, 237900.0, 1.37, 1.37, 0.0};
    inline constexpr CorpoReale Titania  = {"Titania", 788.4, 436300.0, 8.7, 8.7, 0.0};
    inline constexpr CorpoReale Tritone  = {"Tritone", 1353.4, 354800.0, -5.87, -5.87, 0.0};
class Convertitore {
private:
    float distanzaBaseTerra = 45.0f; 
    float raggioBaseTerra   = 0.4f;
    float velocitaBase      = 1.1f; 

public:
    float getDistanza(const CorpoReale& corpo) const {
        if (corpo.distanzaDalPadreKm <= 0.0) return 0.0f;
        
        if (corpo.distanzaDalPadreKm > 3000000.0) { 
            double distNorm = corpo.distanzaDalPadreKm / 149600000.0;
            return (float)(std::pow(distNorm, 0.42) * distanzaBaseTerra);
        } 
        else { 

            double distNorm = corpo.distanzaDalPadreKm / 384400.0;
            
            // - L'offset di 1.2 garantisce che nessuna luna finisca dentro il pianeta padre.
            // - L'esponente 0.6 e il moltiplicatore 2.0 allargano "a ventaglio" le lune di Giove!
            return 1.2f + (float)std::pow(distNorm, 0.6) * 2.0f;
        }
    }

    float getDimensione(const CorpoReale& corpo) const {
        double raggioNorm = corpo.raggioKm / 6371.0;
        float scala = (float)(std::pow(raggioNorm, 0.38) * raggioBaseTerra);
        
        if (corpo.raggioKm > 600000.0) {
            scala *= 1.4f; // Sole
        } 
        else if (corpo.distanzaDalPadreKm > 0.0 && corpo.distanzaDalPadreKm < 3000000.0) {
            scala *= 0.3f; // Lune
        }
        
        return scala;
    }

    float getVelocitaOrbitale(const CorpoReale& corpo) const {
        if (corpo.periodoOrbitaleGiorni == 0.0) return 0.0f;
        float vel = (float)(velocitaBase / corpo.periodoOrbitaleGiorni);
        

        //per le lune
        if (corpo.distanzaDalPadreKm > 0.0 && corpo.distanzaDalPadreKm < 3000000.0) {
            vel *= 0.3f;
        }
        if (vel > 10.0f) vel = 10.0f; 
        if (vel < -10.0f) vel = -10.0f; 
        return vel;
    }

    float getVelocitaRotazione(const CorpoReale& corpo) const {
        if (corpo.periodoRotazioneGiorni == 0.0) return 0.0f;
        float vel = (float)(velocitaBase / corpo.periodoRotazioneGiorni);
        
        if (corpo.distanzaDalPadreKm > 0.0 && corpo.distanzaDalPadreKm < 3000000.0) {
            vel *= 0.3f; 
        }

        if (vel > 10.0f) vel = 10.0f;
        if (vel < -10.0f) vel = -10.0f;
        return vel;
    }

    float getInclinazione(const CorpoReale& corpo) const {
        return glm::radians((float)corpo.inclinazioneGradi);
    }
};
}