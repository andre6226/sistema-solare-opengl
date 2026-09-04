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
        const GLuint vertex = compile(GL_VERTEX_SHADER, readFile(vertexPath), vertexPath);
        GLuint fragment = 0;
        try {
            fragment = compile(GL_FRAGMENT_SHADER, readFile(fragmentPath), fragmentPath);
        } catch (...) {
            glDeleteShader(vertex);
            throw;
        }

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        // The linker has already copied the shader objects into the program, so
        // they can be deleted right away even if linking failed.
        glDeleteShader(vertex);
        glDeleteShader(fragment);

        GLint status = GL_FALSE;
        glGetProgramiv(ID, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            const std::string log = readInfoLog(ID, true);
            glDeleteProgram(ID);
            ID = 0;
            throw std::runtime_error("shader program linking failed:\n" + log);
        }
    }

    ~Shader() {
        if (ID != 0) glDeleteProgram(ID);
    }

    // The program is a unique resource: copying it would mean deleting it twice.
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept : ID(other.ID) {
        other.ID = 0;
    }

    Shader& operator=(Shader&& other) noexcept {
        if (this != &other) {
            if (ID != 0) glDeleteProgram(ID);
            ID = other.ID;
            other.ID = 0;
        }
        return *this;
    }

    void use() const {
        glUseProgram(ID);
    }

    GLint uniformLocation(const char* name) const {
        return glGetUniformLocation(ID, name);
    }

private:
    static std::string readFile(const char* path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error(std::string("could not open shader file '") + path +
                                     "'. Paths are relative to the directory the binary is launched from.");
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    // Fetches the info log of a shader object (isProgram = false) or of a
    // program (isProgram = true). Without this, a GLSL syntax error shows up
    // only as a black window.
    static std::string readInfoLog(GLuint handle, bool isProgram) {
        GLint length = 0;
        if (isProgram) glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &length);
        else           glGetShaderiv(handle,  GL_INFO_LOG_LENGTH, &length);

        if (length <= 0) return "(no message from the driver)";

        std::string log(static_cast<std::size_t>(length), '\0');
        if (isProgram) glGetProgramInfoLog(handle, length, nullptr, log.data());
        else           glGetShaderInfoLog(handle,  length, nullptr, log.data());

        log.resize(log.find_last_not_of('\0') + 1);
        return log;
    }

    static GLuint compile(GLenum type, const std::string& source, const char* path) {
        const GLuint shader = glCreateShader(type);
        const char* code = source.c_str();
        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);

        GLint status = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE) {
            const std::string log = readInfoLog(shader, false);
            glDeleteShader(shader);
            throw std::runtime_error(std::string("compilation of '") + path + "' failed:\n" + log);
        }
        return shader;
    }
};


// Cached uniform locations, so the render loop never calls glGetUniformLocation.
struct UniformLocations {
    // Per frame
    GLint view;
    GLint projection;
    GLint cameraPos;
    GLint lightPos;

    // Per body
    GLint model;
    GLint normalMatrix;
    GLint bodyType;

    // Rings only: geometry of the body casting the shadow onto them.
    GLint parentCenter;
    GLint parentRadius;
};
