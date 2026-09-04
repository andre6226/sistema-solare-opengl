#pragma once
#include "../glad/gl.h"
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

class Shader {
public:
    GLuint ID = 0;

    Shader(const char* vertexPath, const char* fragmentPath) {
        const GLuint vertex   = compila(GL_VERTEX_SHADER,   leggiFile(vertexPath),   vertexPath);
        GLuint fragment = 0;
        try {
            fragment = compila(GL_FRAGMENT_SHADER, leggiFile(fragmentPath), fragmentPath);
        } catch (...) {
            glDeleteShader(vertex);
            throw;
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        // Gli shader object sono gia' stati copiati nel programma dal linker:
        // si possono cancellare subito, anche se il link e' fallito.
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint esito = GL_FALSE;
        glGetProgramiv(ID, GL_LINK_STATUS, &esito);
        if (esito != GL_TRUE) {
            const std::string log = leggiLog(ID, true);
            glDeleteProgram(ID);
            ID = 0;
            throw std::runtime_error("link del programma shader fallito:\n" + log);
        }
    }

    ~Shader() {
        if (ID != 0) glDeleteProgram(ID);
    }

    // Il programma e' una risorsa unica: copiarlo significherebbe cancellarla due volte.
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& altro) noexcept : ID(altro.ID) {
        altro.ID = 0;
    }

    Shader& operator=(Shader&& altro) noexcept {
        if (this != &altro) {
            if (ID != 0) glDeleteProgram(ID);
            ID = altro.ID;
            altro.ID = 0;
        }
        return *this;
    }

    void use() const {
        glUseProgram(ID);
    }

private:
    static std::string leggiFile(const char* percorso) {
        std::ifstream file(percorso);
        if (!file) {
            throw std::runtime_error(std::string("impossibile aprire il file shader '") + percorso +
                                     "'. I percorsi sono relativi alla cartella da cui lanci il binario.");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Recupera l'info log di uno shader object (programma = false) o di un
    // programma (programma = true). Senza questo, un errore di sintassi nel
    // GLSL si manifesta soltanto come una finestra nera.
    static std::string leggiLog(GLuint handle, bool programma) {
        GLint lunghezza = 0;
        if (programma) glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &lunghezza);
        else           glGetShaderiv(handle,  GL_INFO_LOG_LENGTH, &lunghezza);

        if (lunghezza <= 0) return "(nessun messaggio dal driver)";

        std::string log(static_cast<std::size_t>(lunghezza), '\0');
        if (programma) glGetProgramInfoLog(handle, lunghezza, nullptr, log.data());
        else           glGetShaderInfoLog(handle,  lunghezza, nullptr, log.data());

        log.resize(log.find_last_not_of('\0') + 1);
        return log;
    }

    static GLuint compila(GLenum tipo, const std::string& codice, const char* percorso) {
        const GLuint shader = glCreateShader(tipo);
        const char* sorgente = codice.c_str();
        glShaderSource(shader, 1, &sorgente, nullptr);
        glCompileShader(shader);

        GLint esito = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &esito);
        if (esito != GL_TRUE) {
            const std::string log = leggiLog(shader, false);
            glDeleteShader(shader);
            throw std::runtime_error(std::string("compilazione di '") + percorso + "' fallita:\n" + log);
        }
        return shader;
    }
};


struct UniformLocations {
    GLint modello;
    GLint normaleMatrice;
    GLint coloreOggetto;
    GLint isSole;
    GLint vista;
    GLint proiezione;
    GLint cameraPos;
    GLint isAnello;
    GLint isCielo;
};
