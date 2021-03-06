#pragma once

#include <vk_utils/handlers.hpp>
#include <errors/error_handler.hpp>

#include <glm/mat4x4.hpp>

#include <vector>
#include <array>
#include <variant>
#include <unordered_map>
#include <string>

namespace tinyobj
{
    class attrib_t;
    class shape_t;
    class material_t;
}

namespace vk_utils
{
    class obj_loader
    {
    public:
        enum obj_render_technique_type
        {
            PHONG,
            PBR
        };

        enum obj_phong_textures
        {
            PHONG_DIFFUSE,
            PHONG_AMBIENT,
            PHONG_SPECULAR,
            PHONG_SPECULAR_HIGHLIGHT,
            PHONG_BUMP,
            PHONG_DISPLACEMENT,
            PHONG_ALPHA,
            PHONG_REFLECTION,
            PHONG_SIZE,
        };

        enum obj_pbr_textures
        {
            PBR_DIFFUSE,
            PBR_SIZE,
        };

        struct obj_phong_material
        {
            std::array<int32_t, PHONG_SIZE> material_textures{-1, -1, -1, -1, -1, -1, -1, -1};
            std::array<std::array<float, 3>, PHONG_SIZE> material_coefficients{};
        };

        struct obj_pbr_material
        {
        };

        struct obj_sub_geometry
        {
            std::vector<VkFormat> format{};
            uint32_t indices_offset{0};
            uint32_t indices_size{0};
            uint32_t indices_bias{0};
            uint32_t vertices_offset{0};
            uint32_t image_samplers_count{0};
            obj_render_technique_type render_technique{};
            std::variant<obj_phong_material, obj_pbr_material> material = obj_phong_material{};
        };

        struct texture
        {
            vk_utils::vma_image_handler image{};
            vk_utils::image_view_handler image_view{};
            vk_utils::sampler_handler sampler{};
        };

        struct obj_model
        {
            vk_utils::vma_buffer_handler vertex_buffer{};
            vk_utils::vma_buffer_handler index_buffer{};
            std::vector<texture> textures{};
            std::vector<obj_sub_geometry> sub_geometries{};
            std::unordered_map<std::string, uint32_t> other_texturs_key_index_map{};

            glm::mat4 model_transform{1};
        };

        struct  obj_model_info
        {
            std::string model_path{};
            obj_render_technique_type model_render_technique = PHONG;
            std::unordered_map<std::string, std::array<std::string, PHONG_SIZE>> phong_textures;
            std::unordered_map<std::string, std::array<std::string, PBR_SIZE>> pbr_textures;
            std::vector<std::string> other_textures;
        };

        ERROR_TYPE load_model(
            const obj_model_info&,
            VkQueue transfer_queue,
            uint32_t transfer_queue_index,
            VkCommandPool command_pool,
            obj_model&);

    private:
        ERROR_TYPE init_obj_geometry(
            const tinyobj::attrib_t& attrib,
            const std::vector<tinyobj::shape_t>& shapes,
            VkQueue transfer_queue,
            uint32_t transfer_queue_index,
            VkCommandPool command_pool,
            obj_model& model);

        ERROR_TYPE init_obj_materials(
            const std::vector<tinyobj::shape_t>& shapes,
            const std::vector<tinyobj::material_t>& materials,
            const obj_model_info& model_info,
            VkQueue transfer_queue,
            uint32_t transfer_queue_index,
            VkCommandPool command_pool,
            obj_model& model);
    };

}

