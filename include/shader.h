#pragma once
#ifndef SHADER_H
#define SHADER_H

#include <string>

#include "glm.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#if defined __has_include
    #if __has_include (<SDL_opengl.h>)
        #include <SDL_opengl.h>
    #else
        #include <GL/gl.h>
    #endif
#endif

class _shader
{
public:
    _shader();
    _shader(std::string vertex, std::string fragment);
    ~_shader();

    bool load(std::string vertex, std::string fragment);
    //bool generate(tinyobj::material_t material, std::vector<std::string> layout);
    void use();
    //GLint ID() {return programID;};

    void addVertFile(std::string path);
    void addVertRaw(std::string raw);

	//void setJsonValues(Json::Value inshadervalues);
	//void updateJsonValues();

    std::string getErrors();

    void setBool(const std::string &name, bool value) const;
    void setInt(const std::string &name, int value) const;
    void setFloat(const std::string &name, float value) const;
    void setVec2(const std::string &name, const glm::vec2 &value) const;
    void setVec2(const std::string &name, float x, float y) const;
    void setVec3(const std::string &name, const glm::vec3 &value) const;
    void setVec3(const std::string &name, float x, float y, float z) const;
    void setVec4(const std::string &name, const glm::vec4 &value) const;
    void setVec4(const std::string &name, float x, float y, float z, float w);
    void setMat2(const std::string &name, const glm::mat2 &mat) const;
    void setMat3(const std::string &name, const glm::mat3 &mat) const;
    void setMat4(const std::string &name, const glm::mat4 &mat) const;

private:
    std::string readFile(std::string path);
    bool compile(std::string vertexsource, std::string fragmentsource);
    bool checkCompileErrors(GLuint shader, std::string type);

	//Json::Value shadervalues;

    std::stringstream errorlog;

    std::string loadedShaderFunctions;

	uint programID;
    bool run;
};

#endif