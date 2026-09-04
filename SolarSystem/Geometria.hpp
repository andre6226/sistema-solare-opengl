#pragma once

// Interfaccia comune alle primitive che possiedono risorse OpenGL (VAO/VBO/EBO).
//
// Copia e spostamento sono vietati a livello di base: gli handle OpenGL sono
// identificatori unici, e una copia porterebbe il duplicato a chiamare
// glDelete* su risorse ancora usate dall'originale (double free lato driver).
// Vietandoli qui, tutte le classi derivate li ereditano gia' cancellati.
class Geometria {
public:
    Geometria() = default;
    virtual ~Geometria() = default;

    Geometria(const Geometria&)            = delete;
    Geometria& operator=(const Geometria&) = delete;
    Geometria(Geometria&&)                 = delete;
    Geometria& operator=(Geometria&&)      = delete;

    virtual void draw() = 0;
};
