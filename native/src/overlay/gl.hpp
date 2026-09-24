#pragma once

#include <cstddef>

#include <GLFW/glfw3.h>

/// The handful of OpenGL 3.3 entry points the model renderer needs.
///
/// GLFW only declares the 1.1 ones, and ImGui keeps its loader to itself, so these are
/// resolved by hand rather than pulling in a whole loader library for twenty functions.
namespace dl::overlay::gl {

using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;
using GLintptr = std::ptrdiff_t;

/// Resolves every pointer below. Returns false when the driver is missing any of them,
/// which means the context is older than the shaders need.
bool load();

extern void (*GenVertexArrays)(GLsizei, GLuint*);
extern void (*DeleteVertexArrays)(GLsizei, const GLuint*);
extern void (*BindVertexArray)(GLuint);
extern void (*GenBuffers)(GLsizei, GLuint*);
extern void (*DeleteBuffers)(GLsizei, const GLuint*);
extern void (*BindBuffer)(GLenum, GLuint);
extern void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum);
extern void (*EnableVertexAttribArray)(GLuint);
extern void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
extern void (*VertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void*);
extern GLuint (*CreateShader)(GLenum);
extern void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
extern void (*CompileShader)(GLuint);
extern void (*GetShaderiv)(GLuint, GLenum, GLint*);
extern void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*DeleteShader)(GLuint);
extern GLuint (*CreateProgram)();
extern void (*AttachShader)(GLuint, GLuint);
extern void (*LinkProgram)(GLuint);
extern void (*GetProgramiv)(GLuint, GLenum, GLint*);
extern void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
extern void (*UseProgram)(GLuint);
extern void (*DeleteProgram)(GLuint);
extern GLint (*GetUniformLocation)(GLuint, const GLchar*);
extern void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
extern void (*Uniform4fv)(GLint, GLsizei, const GLfloat*);
extern void (*Uniform1fv)(GLint, GLsizei, const GLfloat*);

}  // namespace dl::overlay::gl
