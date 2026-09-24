#include "overlay/gl.hpp"

#include <cstdio>

namespace dl::overlay::gl {

decltype(GenVertexArrays) GenVertexArrays = nullptr;
decltype(DeleteVertexArrays) DeleteVertexArrays = nullptr;
decltype(BindVertexArray) BindVertexArray = nullptr;
decltype(GenBuffers) GenBuffers = nullptr;
decltype(DeleteBuffers) DeleteBuffers = nullptr;
decltype(BindBuffer) BindBuffer = nullptr;
decltype(BufferData) BufferData = nullptr;
decltype(EnableVertexAttribArray) EnableVertexAttribArray = nullptr;
decltype(VertexAttribPointer) VertexAttribPointer = nullptr;
decltype(VertexAttribIPointer) VertexAttribIPointer = nullptr;
decltype(CreateShader) CreateShader = nullptr;
decltype(ShaderSource) ShaderSource = nullptr;
decltype(CompileShader) CompileShader = nullptr;
decltype(GetShaderiv) GetShaderiv = nullptr;
decltype(GetShaderInfoLog) GetShaderInfoLog = nullptr;
decltype(DeleteShader) DeleteShader = nullptr;
decltype(CreateProgram) CreateProgram = nullptr;
decltype(AttachShader) AttachShader = nullptr;
decltype(LinkProgram) LinkProgram = nullptr;
decltype(GetProgramiv) GetProgramiv = nullptr;
decltype(GetProgramInfoLog) GetProgramInfoLog = nullptr;
decltype(UseProgram) UseProgram = nullptr;
decltype(DeleteProgram) DeleteProgram = nullptr;
decltype(GetUniformLocation) GetUniformLocation = nullptr;
decltype(UniformMatrix4fv) UniformMatrix4fv = nullptr;
decltype(Uniform4fv) Uniform4fv = nullptr;
decltype(Uniform1fv) Uniform1fv = nullptr;

namespace {

bool resolve(void** slot, const char* name) {
    *slot = reinterpret_cast<void*>(glfwGetProcAddress(name));
    if (*slot == nullptr) {
        std::fprintf(stderr, "opengl entry point %s is missing\n", name);
        return false;
    }
    return true;
}

}  // namespace

bool load() {
    bool ok = true;
    ok = resolve(reinterpret_cast<void**>(&GenVertexArrays), "glGenVertexArrays") && ok;
    ok = resolve(reinterpret_cast<void**>(&DeleteVertexArrays), "glDeleteVertexArrays") && ok;
    ok = resolve(reinterpret_cast<void**>(&BindVertexArray), "glBindVertexArray") && ok;
    ok = resolve(reinterpret_cast<void**>(&GenBuffers), "glGenBuffers") && ok;
    ok = resolve(reinterpret_cast<void**>(&DeleteBuffers), "glDeleteBuffers") && ok;
    ok = resolve(reinterpret_cast<void**>(&BindBuffer), "glBindBuffer") && ok;
    ok = resolve(reinterpret_cast<void**>(&BufferData), "glBufferData") && ok;
    ok = resolve(reinterpret_cast<void**>(&EnableVertexAttribArray), "glEnableVertexAttribArray") && ok;
    ok = resolve(reinterpret_cast<void**>(&VertexAttribPointer), "glVertexAttribPointer") && ok;
    ok = resolve(reinterpret_cast<void**>(&VertexAttribIPointer), "glVertexAttribIPointer") && ok;
    ok = resolve(reinterpret_cast<void**>(&CreateShader), "glCreateShader") && ok;
    ok = resolve(reinterpret_cast<void**>(&ShaderSource), "glShaderSource") && ok;
    ok = resolve(reinterpret_cast<void**>(&CompileShader), "glCompileShader") && ok;
    ok = resolve(reinterpret_cast<void**>(&GetShaderiv), "glGetShaderiv") && ok;
    ok = resolve(reinterpret_cast<void**>(&GetShaderInfoLog), "glGetShaderInfoLog") && ok;
    ok = resolve(reinterpret_cast<void**>(&DeleteShader), "glDeleteShader") && ok;
    ok = resolve(reinterpret_cast<void**>(&CreateProgram), "glCreateProgram") && ok;
    ok = resolve(reinterpret_cast<void**>(&AttachShader), "glAttachShader") && ok;
    ok = resolve(reinterpret_cast<void**>(&LinkProgram), "glLinkProgram") && ok;
    ok = resolve(reinterpret_cast<void**>(&GetProgramiv), "glGetProgramiv") && ok;
    ok = resolve(reinterpret_cast<void**>(&GetProgramInfoLog), "glGetProgramInfoLog") && ok;
    ok = resolve(reinterpret_cast<void**>(&UseProgram), "glUseProgram") && ok;
    ok = resolve(reinterpret_cast<void**>(&DeleteProgram), "glDeleteProgram") && ok;
    ok = resolve(reinterpret_cast<void**>(&GetUniformLocation), "glGetUniformLocation") && ok;
    ok = resolve(reinterpret_cast<void**>(&UniformMatrix4fv), "glUniformMatrix4fv") && ok;
    ok = resolve(reinterpret_cast<void**>(&Uniform4fv), "glUniform4fv") && ok;
    ok = resolve(reinterpret_cast<void**>(&Uniform1fv), "glUniform1fv") && ok;
    return ok;
}

}  // namespace dl::overlay::gl
