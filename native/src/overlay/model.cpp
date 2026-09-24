#include "overlay/model.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "overlay/gl.hpp"

namespace dl::overlay {
namespace {

/// how many joints the shader has room for, and what the game's skeletons hold
constexpr std::size_t max_bones = 96;

/// The models are authored in metres and the game works in inches, so every bind pose has
/// to be scaled on the way in.
constexpr float model_units_to_source = 39.3701f;

const char* vertex_shader = R"(#version 330 core

layout (location = 0) in vec3 a_position;
layout (location = 1) in uvec4 a_joints;
layout (location = 2) in vec4 a_weights;

uniform mat4 u_view;
uniform mat4 u_model;
uniform mat4 u_bones[96];
uniform float u_bone_visibility[96];

out float v_visibility;

void main() {
    mat4 skin =
        a_weights.x * u_bones[a_joints.x] +
        a_weights.y * u_bones[a_joints.y] +
        a_weights.z * u_bones[a_joints.z] +
        a_weights.w * u_bones[a_joints.w];

    v_visibility =
        a_weights.x * u_bone_visibility[a_joints.x] +
        a_weights.y * u_bone_visibility[a_joints.y] +
        a_weights.z * u_bone_visibility[a_joints.z] +
        a_weights.w * u_bone_visibility[a_joints.w];

    gl_Position = u_view * u_model * skin * vec4(a_position, 1.0);
}
)";

const char* fragment_shader = R"(#version 330 core

uniform vec4 u_visible_color;
uniform vec4 u_invisible_color;
in float v_visibility;
out vec4 frag_color;

void main() {
    frag_color = mix(u_invisible_color, u_visible_color, clamp(v_visibility, 0.0, 1.0));
}
)";

/// The game names a model by its full path, and several variants share one mesh. The rust
/// client's reduced set drops the variant suffix, so "tm_leet_variantf" becomes "tm_leet".
std::string model_key(const std::string& path) {
    std::string name = std::filesystem::path(path).stem().string();
    for (const std::string_view suffix : {"_variant", "_var"}) {
        const std::size_t at = name.find(suffix);
        if (at != std::string::npos) {
            name.resize(at);
            break;
        }
    }
    return name;
}

unsigned compile(GLenum type, const char* source, std::string& error) {
    const unsigned shader = gl::CreateShader(type);
    gl::ShaderSource(shader, 1, &source, nullptr);
    gl::CompileShader(shader);

    GLint status = 0;
    gl::GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        std::array<char, 1024> log{};
        gl::GetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        error = log.data();
        gl::DeleteShader(shader);
        return 0;
    }
    return shader;
}

/// Reads an accessor into a flat float array, whatever component type it uses.
std::vector<float> read_floats(const cgltf_accessor* accessor, std::size_t components) {
    std::vector<float> out;
    if (accessor == nullptr) {
        return out;
    }
    out.resize(accessor->count * components);
    for (std::size_t i = 0; i < accessor->count; ++i) {
        cgltf_accessor_read_float(accessor, i, out.data() + i * components,
                                  static_cast<cgltf_size>(components));
    }
    return out;
}

}  // namespace

ModelRenderer::~ModelRenderer() {
    for (auto& [name, mesh] : meshes_) {
        destroy_mesh(mesh);
    }
    if (program_ != 0) {
        gl::DeleteProgram(program_);
    }
}

std::string ModelRenderer::create() {
    if (!gl::load()) {
        return "this opengl context is missing entry points the model shaders need";
    }

    std::string error;
    const unsigned vertex = compile(GL_VERTEX_SHADER, vertex_shader, error);
    if (vertex == 0) {
        return "vertex shader: " + error;
    }
    const unsigned fragment = compile(GL_FRAGMENT_SHADER, fragment_shader, error);
    if (fragment == 0) {
        gl::DeleteShader(vertex);
        return "fragment shader: " + error;
    }

    program_ = gl::CreateProgram();
    gl::AttachShader(program_, vertex);
    gl::AttachShader(program_, fragment);
    gl::LinkProgram(program_);
    gl::DeleteShader(vertex);
    gl::DeleteShader(fragment);

    GLint status = 0;
    gl::GetProgramiv(program_, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        std::array<char, 1024> log{};
        gl::GetProgramInfoLog(program_, static_cast<GLsizei>(log.size()), nullptr, log.data());
        gl::DeleteProgram(program_);
        program_ = 0;
        return std::string("linking the model program: ") + log.data();
    }

    uniform_view_ = gl::GetUniformLocation(program_, "u_view");
    uniform_model_ = gl::GetUniformLocation(program_, "u_model");
    uniform_visible_ = gl::GetUniformLocation(program_, "u_visible_color");
    uniform_invisible_ = gl::GetUniformLocation(program_, "u_invisible_color");
    uniform_bones_ = gl::GetUniformLocation(program_, "u_bones[0]");
    uniform_bone_visibility_ = gl::GetUniformLocation(program_, "u_bone_visibility[0]");
    return {};
}

void ModelRenderer::destroy_mesh(Mesh& mesh) {
    for (Primitive& primitive : mesh.primitives) {
        gl::DeleteVertexArrays(1, &primitive.vao);
        const std::array<unsigned, 4> buffers{primitive.position_buffer, primitive.joint_buffer,
                                              primitive.weight_buffer, primitive.index_buffer};
        gl::DeleteBuffers(static_cast<GLsizei>(buffers.size()), buffers.data());
    }
    mesh.primitives.clear();
}

const ModelRenderer::Mesh* ModelRenderer::mesh_for(const std::string& key) {
    if (const auto found = meshes_.find(key); found != meshes_.end()) {
        return &found->second;
    }
    if (missing_.contains(key)) {
        return nullptr;
    }

#ifndef DL_ASSETS_DIR
    missing_.emplace(key, true);
    return nullptr;
#else
    const std::filesystem::path path =
        std::filesystem::path(DL_ASSETS_DIR) / "models" / (key + ".glb");
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
        missing_.emplace(key, true);
        return nullptr;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, path.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "could not parse %s\n", path.c_str());
        missing_.emplace(key, true);
        return nullptr;
    }
    if (cgltf_load_buffers(&options, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        missing_.emplace(key, true);
        return nullptr;
    }

    Mesh mesh;
    for (cgltf_size m = 0; m < data->meshes_count; ++m) {
        const cgltf_mesh& source = data->meshes[m];

        // the skin that drives this mesh, found through whichever node uses it
        const cgltf_skin* skin = nullptr;
        for (cgltf_size n = 0; n < data->nodes_count; ++n) {
            if (data->nodes[n].mesh == &source && data->nodes[n].skin != nullptr) {
                skin = data->nodes[n].skin;
                break;
            }
        }
        if (skin == nullptr) {
            continue;
        }

        std::vector<Mat4> inverse_bind(skin->joints_count, Mat4(1.0f));
        if (skin->inverse_bind_matrices != nullptr) {
            const std::vector<float> values = read_floats(skin->inverse_bind_matrices, 16);
            for (std::size_t j = 0; j < skin->joints_count && (j + 1) * 16 <= values.size(); ++j) {
                // glm keeps its storage private, so the columns are assigned rather
                // than copied over in one go
                for (int column = 0; column < 4; ++column) {
                    for (int row = 0; row < 4; ++row) {
                        inverse_bind[j][column][row] =
                            values[j * 16 + static_cast<std::size_t>(column * 4 + row)];
                    }
                }
            }
        }

        for (cgltf_size p = 0; p < source.primitives_count; ++p) {
            const cgltf_primitive& source_primitive = source.primitives[p];
            const cgltf_accessor* positions = nullptr;
            const cgltf_accessor* joints = nullptr;
            const cgltf_accessor* weights = nullptr;
            for (cgltf_size a = 0; a < source_primitive.attributes_count; ++a) {
                const cgltf_attribute& attribute = source_primitive.attributes[a];
                switch (attribute.type) {
                    case cgltf_attribute_type_position: positions = attribute.data; break;
                    case cgltf_attribute_type_joints: joints = attribute.data; break;
                    case cgltf_attribute_type_weights: weights = attribute.data; break;
                    default: break;
                }
            }
            if (positions == nullptr || joints == nullptr || weights == nullptr ||
                source_primitive.indices == nullptr) {
                continue;
            }

            const std::vector<float> position_data = read_floats(positions, 3);
            const std::vector<float> weight_data = read_floats(weights, 4);

            std::vector<std::uint32_t> joint_data(joints->count * 4, 0);
            for (cgltf_size i = 0; i < joints->count; ++i) {
                cgltf_uint quad[4]{};
                cgltf_accessor_read_uint(joints, i, quad, 4);
                for (int c = 0; c < 4; ++c) {
                    // a joint past what the shader can address would read past its array
                    joint_data[i * 4 + static_cast<std::size_t>(c)] =
                        quad[c] < max_bones ? quad[c] : 0;
                }
            }

            std::vector<std::uint32_t> indices(source_primitive.indices->count);
            for (cgltf_size i = 0; i < source_primitive.indices->count; ++i) {
                indices[i] = static_cast<std::uint32_t>(
                    cgltf_accessor_read_index(source_primitive.indices, i));
            }

            Primitive primitive;
            primitive.index_count = static_cast<int>(indices.size());
            primitive.joint_count = skin->joints_count;
            primitive.inverse_bind = inverse_bind;

            gl::GenVertexArrays(1, &primitive.vao);
            gl::BindVertexArray(primitive.vao);

            const auto upload = [](unsigned& buffer, GLenum target, const void* bytes,
                                   std::size_t size) {
                gl::GenBuffers(1, &buffer);
                gl::BindBuffer(target, buffer);
                gl::BufferData(target, static_cast<gl::GLsizeiptr>(size), bytes, GL_STATIC_DRAW);
            };

            upload(primitive.position_buffer, GL_ARRAY_BUFFER, position_data.data(),
                   position_data.size() * sizeof(float));
            gl::EnableVertexAttribArray(0);
            gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

            upload(primitive.joint_buffer, GL_ARRAY_BUFFER, joint_data.data(),
                   joint_data.size() * sizeof(std::uint32_t));
            gl::EnableVertexAttribArray(1);
            gl::VertexAttribIPointer(1, 4, GL_UNSIGNED_INT, 0, nullptr);

            upload(primitive.weight_buffer, GL_ARRAY_BUFFER, weight_data.data(),
                   weight_data.size() * sizeof(float));
            gl::EnableVertexAttribArray(2);
            gl::VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

            upload(primitive.index_buffer, GL_ELEMENT_ARRAY_BUFFER, indices.data(),
                   indices.size() * sizeof(std::uint32_t));

            gl::BindVertexArray(0);
            mesh.primitives.push_back(std::move(primitive));
        }
    }
    cgltf_free(data);

    if (mesh.primitives.empty()) {
        missing_.emplace(key, true);
        return nullptr;
    }
    std::fprintf(stderr, "loaded model %s, %zu primitives\n", key.c_str(),
                 mesh.primitives.size());
    return &meshes_.emplace(key, std::move(mesh)).first->second;
#endif
}

void ModelRenderer::draw(const std::string& model_name,
                         const std::vector<cs2::BoneTransform>& skeleton, const Mat4& view,
                         const ui::Color& visible, const ui::Color& invisible,
                         config::ModelRenderMode mode) {
    if (program_ == 0 || skeleton.empty()) {
        return;
    }
    const Mesh* mesh = mesh_for(model_key(model_name));
    if (mesh == nullptr) {
        return;
    }

    gl::UseProgram(program_);
    gl::UniformMatrix4fv(uniform_view_, 1, GL_FALSE, &view[0][0]);
    const Mat4 identity(1.0f);
    gl::UniformMatrix4fv(uniform_model_, 1, GL_FALSE, &identity[0][0]);

    const ImVec4 visible_parts = visible.vec4();
    const ImVec4 invisible_parts = invisible.vec4();
    gl::Uniform4fv(uniform_visible_, 1, &visible_parts.x);
    gl::Uniform4fv(uniform_invisible_, 1, &invisible_parts.x);

    std::array<float, max_bones> visibility{};
    for (std::size_t i = 0; i < max_bones; ++i) {
        visibility[i] = i < skeleton.size() ? skeleton[i].visibility : 0.0f;
    }
    gl::Uniform1fv(uniform_bone_visibility_, static_cast<GLsizei>(max_bones), visibility.data());

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const Primitive& primitive : mesh->primitives) {
        // one matrix per joint: where the bone is now, undoing the bind pose it was
        // modelled in, with the metres to inches correction in between
        std::array<Mat4, max_bones> palette;
        palette.fill(Mat4(1.0f));
        const Mat4 correction = glm::scale(Mat4(1.0f), Vec3(model_units_to_source));
        for (std::size_t j = 0; j < std::min(primitive.joint_count, max_bones); ++j) {
            if (j >= skeleton.size()) {
                break;
            }
            const Mat4 bind = j < primitive.inverse_bind.size() ? primitive.inverse_bind[j]
                                                                : Mat4(1.0f);
            palette[j] = skeleton[j].matrix * correction * bind;
        }
        gl::UniformMatrix4fv(uniform_bones_, static_cast<GLsizei>(max_bones), GL_FALSE,
                             &palette[0][0][0]);

        glPolygonMode(GL_FRONT_AND_BACK,
                      mode == config::ModelRenderMode::Wireframe ? GL_LINE : GL_FILL);
        gl::BindVertexArray(primitive.vao);
        glDrawElements(GL_TRIANGLES, primitive.index_count, GL_UNSIGNED_INT, nullptr);
    }

    gl::BindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_DEPTH_TEST);
    gl::UseProgram(0);
}

}  // namespace dl::overlay
