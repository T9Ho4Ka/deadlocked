#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "config/game.hpp"
#include "cs2/entity.hpp"
#include "math.hpp"

namespace dl::overlay {

/// Draws the players' own models over the game, tinted by whether each bone can be seen.
///
/// The models are the game's, read from the asset directory the rust client embeds. They
/// are loaded the first time a player wearing one shows up rather than all at once: the
/// full set is ninety megabytes, and a round only ever uses a handful of them.
class ModelRenderer {
public:
    ModelRenderer() = default;
    ModelRenderer(const ModelRenderer&) = delete;
    ModelRenderer& operator=(const ModelRenderer&) = delete;
    ~ModelRenderer();

    /// Compiles the shaders. Returns an empty string on success, the reason otherwise.
    [[nodiscard]] std::string create();
    [[nodiscard]] bool ready() const { return program_ != 0; }

    /// Draws one player. `model_name` is the path the game reports, whose last component
    /// names the model; anything unknown is skipped.
    void draw(const std::string& model_name, const std::vector<cs2::BoneTransform>& skeleton,
              const Mat4& view, const ui::Color& visible, const ui::Color& invisible,
              config::ModelRenderMode mode);

private:
    struct Primitive {
        unsigned vao = 0;
        unsigned position_buffer = 0;
        unsigned joint_buffer = 0;
        unsigned weight_buffer = 0;
        unsigned index_buffer = 0;
        int index_count = 0;
        std::size_t joint_count = 0;
        std::vector<Mat4> inverse_bind;
    };

    struct Mesh {
        std::vector<Primitive> primitives;
    };

    /// Loads a model from the asset directory, or hands back one already loaded. Null when
    /// there is no such model, which is remembered so the file is not looked for again.
    const Mesh* mesh_for(const std::string& key);
    void destroy_mesh(Mesh& mesh);

    unsigned program_ = 0;
    int uniform_view_ = -1;
    int uniform_model_ = -1;
    int uniform_visible_ = -1;
    int uniform_invisible_ = -1;
    int uniform_bones_ = -1;
    int uniform_bone_visibility_ = -1;

    std::unordered_map<std::string, Mesh> meshes_;
    /// models that are not on disk, so the miss is only paid once
    std::unordered_map<std::string, bool> missing_;
};

}  // namespace dl::overlay
