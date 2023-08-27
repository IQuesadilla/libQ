#define GL_GLEXT_PROTOTYPES 1

#include "shader.h"
#include <algorithm>

_shader::_shader()
{
    programID = 0;
}

_shader::_shader(std::string vertex, std::string fragment)
{
    _shader();

    load(vertex, fragment);
}

_shader::~_shader()
{
    glDeleteProgram(programID);
}

bool _shader::load(std::string vertex, std::string fragment)
{
    std::string vertexsource = readFile(vertex);
    std::string fragmentsource = readFile(fragment);

    bool error = compile(vertexsource, fragmentsource);
    if (error)
    {
        errorlog << "Failed to load V:" << vertex << " and F:" << fragment << std::endl;
    }
    return error;
}
/*
bool _shader::generate(tinyobj::material_t material, std::vector<std::string> layout)
{
    std::stringstream vertexsource, fragmentsource;

    vertexsource << "#version 330 core" << std::endl;
    fragmentsource << "#version 330 core" << std::endl;

    for (int x = 0; x < layout.size(); x++)
    {
        vertexsource << "layout (location = " << x << ") in " << layout.at(x) << " LAY" << x << ';' << std::endl;
    }
    vertexsource << std::endl;

    for (int x = 0; x < layout.size(); x++)
    {
        vertexsource << "out " << layout.at(x) << " VTF" << x << ';' << std::endl;
        fragmentsource << "in " << layout.at(x) << " VTF" << x << ';' << std::endl;
    }

    vertexsource << std::endl << "uniform mat4 projection;" << std::endl << "uniform mat4 view;" << std::endl << "uniform mat4 model;" << std::endl;

    return true;
}
*/
void _shader::use()
{
    glUseProgram (programID);
}

void _shader::addVertFile(std::string path)
{
    std::string raw = readFile(path);
    return addVertRaw(raw);
}

void _shader::addVertRaw(std::string raw)
{
    loadedShaderFunctions += (raw + '\n');
}

/*
void _shader::setJsonValues(Json::Value inshadervalues)
{
	shadervalues = inshadervalues;
}

void _shader::updateJsonValues()
{
	shadervalues.getMemberNames();
}
*/

std::string _shader::getErrors()
{
    std::string buff = errorlog.str();
    errorlog = std::stringstream();

    std::size_t first = buff.find_first_not_of("\r\n ");
    std::size_t last = buff.find_last_not_of("\r\n ");
    return buff.substr(first,(last-first+1));
}

void _shader::setBool(const std::string &name, bool value) const
{
    glUniform1i(glGetUniformLocation(programID, name.c_str()), (int)value);
}

void _shader::setInt(const std::string &name, int value) const
{
    glUniform1i(glGetUniformLocation(programID, name.c_str()), value);
}

void _shader::setFloat(const std::string &name, float value) const
{
    glUniform1f(glGetUniformLocation(programID, name.c_str()), value);
}

void _shader::setVec2(const std::string &name, const glm::vec2 &value) const
{
    glUniform2fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
}

void _shader::setVec2(const std::string &name, float x, float y) const
{
    glUniform2f(glGetUniformLocation(programID, name.c_str()), x, y);
}

void _shader::setVec3(const std::string &name, const glm::vec3 &value) const
{
    glUniform3fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
}

void _shader::setVec3(const std::string &name, float x, float y, float z) const
{
    glUniform3f(glGetUniformLocation(programID, name.c_str()), x, y, z);
}
void _shader::setVec4(const std::string &name, const glm::vec4 &value) const
{
    glUniform4fv(glGetUniformLocation(programID, name.c_str()), 1, &value[0]);
}
void _shader::setVec4(const std::string &name, float x, float y, float z, float w)
{
    glUniform4f(glGetUniformLocation(programID, name.c_str()), x, y, z, w);
}

void _shader::setMat2(const std::string &name, const glm::mat2 &mat) const
{
    glUniformMatrix2fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void _shader::setMat3(const std::string &name, const glm::mat3 &mat) const
{
    glUniformMatrix3fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

void _shader::setMat4(const std::string &name, const glm::mat4 &mat) const
{
    glUniformMatrix4fv(glGetUniformLocation(programID, name.c_str()), 1, GL_FALSE, &mat[0][0]);
}

std::string _shader::readFile(std::string path)
{
    std::ifstream file;
    file.open(path);

    if (!file.is_open())
    {
        errorlog << "Failed to read file \"" << path << "\"!" << std::endl;
        return "";
    }

    std::string data = std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
    file.close();
    return data;
}

bool _shader::compile(std::string vertexsource, std::string fragmentsource)
{
    bool error = false;

    auto loc = vertexsource.find("void main");
    vertexsource.insert(loc, '\n' + loadedShaderFunctions + '\n');

    const char *vertexdata = vertexsource.c_str();
    const char *fragmentdata = fragmentsource.c_str();

    GLuint vshader = glCreateShader (GL_VERTEX_SHADER);
	glShaderSource (vshader, 1, &vertexdata, NULL);
	glCompileShader (vshader);
    error |= checkCompileErrors(vshader,"VERTEX");

	GLuint fshader = glCreateShader (GL_FRAGMENT_SHADER);
	glShaderSource (fshader, 1, &fragmentdata, NULL);
	glCompileShader (fshader);
    error |= checkCompileErrors(fshader,"FRAGMENT");

    if ( !error )
    {
        programID = glCreateProgram ();
        glAttachShader (programID, vshader);
        glAttachShader (programID, fshader);
        glLinkProgram (programID);
        error = checkCompileErrors(programID,"PROGRAM");
    }

    glDeleteShader (vshader);
	glDeleteShader (fshader);

    return error;
}

bool _shader::checkCompileErrors(GLuint shader, std::string type)
{
    GLint success;
    GLchar infoLog[1024];
    if(type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            errorlog << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << " -> " << infoLog;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            errorlog << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << " -> " << infoLog;
        }
    }
    return !success;
}
