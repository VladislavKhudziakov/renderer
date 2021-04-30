
#include "obj_loader.hpp"

#include <vk_utils/tools.hpp>
#include <vk_utils/context.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <glm/gtc/matrix_transform.hpp>

#include <optional>
#include <array>
#include <vector>
#include <cstring>
#include <filesystem>

ERROR_TYPE vk_utils::obj_loader::load_model(
    const char* path,
    const vk_utils::obj_loader::obj_model_info& model_info,
    VkQueue transfer_queue,
    VkCommandPool command_pool,
    vk_utils::obj_loader::obj_model& model)
{
    if (model_info.model_render_technique != PHONG) {
        RAISE_ERROR_WARN(-1, "unsupported render technique.");
    }

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(path)) {
        RAISE_ERROR_FATAL(-1, reader.Error());
    }

    model.path = path;

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

    if (!reader.Warning().empty()) {
        LOG_WARN(reader.Warning());
    }

    PASS_ERROR(init_obj_geometry(attrib, shapes, transfer_queue, command_pool, model));
    PASS_ERROR(init_obj_materials(shapes, materials, model_info, transfer_queue, command_pool, model));
    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::obj_loader::init_obj_geometry(
    const tinyobj::attrib_t& attrib,
    const std::vector<tinyobj::shape_t>& shapes,
    VkQueue transfer_queue,
    VkCommandPool command_pool,
    obj_model& model)
{
    struct vertex
    {
        glm::vec3 position;
        std::optional<glm::vec3> normal;
        std::optional<glm::vec2> texcoord;
    };

    struct geometry
    {
        std::vector<vertex> vertices;
        std::vector<uint32_t> indices;
        std::vector<VkFormat> vertex_format;
        size_t vertex_size = 0;
    };

    std::vector<geometry> geometries;
    geometries.reserve(shapes.size());

    auto vert_eq = [](const vertex& v1) {
        return [&v1](const vertex& v2) {
            if (v1.position != v2.position) {
                return false;
            }
            if (v1.normal && v2.normal && (*v1.normal != *v2.normal)) {
                return false;
            }
            if (v1.texcoord && v2.texcoord && (*v1.texcoord != *v2.texcoord)) {
                return false;
            }
            return true;
        };
    };

    glm::vec3 max_pos{std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()};
    glm::vec3 min_pos{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};

    for (const auto& shape : shapes) {
        auto& g = geometries.emplace_back();
        g.vertices.reserve(shape.mesh.indices.size());
        auto& i = shape.mesh.indices.front();

        g.vertex_format.emplace_back(VK_FORMAT_R32G32B32_SFLOAT);
        g.vertex_size += sizeof(float[3]);

        if (i.normal_index >= 0) {
            g.vertex_format.emplace_back(VK_FORMAT_R32G32B32_SFLOAT);
            g.vertex_size += sizeof(float[3]);
        }

        if (i.texcoord_index >= 0) {
            g.vertex_format.emplace_back(VK_FORMAT_R32G32_SFLOAT);
            g.vertex_size += sizeof(float[2]);
        }

        for (auto& i : shape.mesh.indices) {
            vertex v{};

            v.position[0] = attrib.vertices[3 * i.vertex_index];
            v.position[1] = attrib.vertices[3 * i.vertex_index + 1];
            v.position[2] = attrib.vertices[3 * i.vertex_index + 2];

            max_pos.x = std::max(max_pos.x, v.position[0]);
            max_pos.y = std::max(max_pos.y, v.position[1]);
            max_pos.z = std::max(max_pos.z, v.position[2]);

            min_pos.x = std::min(min_pos.x, v.position[0]);
            min_pos.y = std::min(min_pos.y, v.position[1]);
            min_pos.z = std::min(min_pos.z, v.position[2]);

            if (i.normal_index >= 0) {
                v.normal = {attrib.normals[3 * i.normal_index], attrib.normals[3 * i.normal_index + 1], attrib.normals[3 * i.normal_index + 2]};
            }

            if (i.texcoord_index >= 0) {
                v.texcoord = glm::vec2{attrib.texcoords[2 * i.texcoord_index], attrib.texcoords[2 * i.texcoord_index + 1]};
            }

            auto vertex_exist = std::find_if(g.vertices.begin(), g.vertices.end(), vert_eq(v));

            if (vertex_exist != g.vertices.end()) {
                g.indices.push_back(std::distance(g.vertices.begin(), vertex_exist));
            } else {
                g.indices.push_back(g.vertices.size());
                g.vertices.emplace_back(v);
            }
        }
    }
    size_t vert_values_count = 0;
    size_t indices_count = 0;

    std::vector<float> vert_buffer_data;
    std::vector<uint32_t> index_buffer_data;

    for (auto& g : geometries) {
        size_t vertex_floats_count = 3;

        if (g.vertices.front().normal) {
            vertex_floats_count += 3;
        }

        if (g.vertices.front().texcoord) {
            vertex_floats_count += 2;
        }

        vert_values_count += vertex_floats_count * g.vertices.size();
        indices_count += g.indices.size();
    }

    vert_buffer_data.reserve(vert_values_count);
    index_buffer_data.reserve(indices_count);
    uint32_t start_vertex = 0;
    size_t indices_offset = 0;
    size_t vertices_offset = 0;

    model.sub_geometries.reserve(geometries.size());

    for (auto& geometry : geometries) {
        auto& sub_geometry = model.sub_geometries.emplace_back();
        sub_geometry.format = geometry.vertex_format;
        sub_geometry.indices_offset = indices_offset;
        sub_geometry.vertices_offset = vertices_offset;
        sub_geometry.indices_bias = start_vertex;
        sub_geometry.indices_size = geometry.indices.size();

        for (const auto& v : geometry.vertices) {
            vert_buffer_data.push_back(v.position[0]);
            vert_buffer_data.push_back(v.position[1]);
            vert_buffer_data.push_back(v.position[2]);

            if (v.normal) {
                vert_buffer_data.push_back(v.normal->x);
                vert_buffer_data.push_back(v.normal->y);
                vert_buffer_data.push_back(v.normal->z);
            }

            if (v.texcoord) {
                vert_buffer_data.push_back(v.texcoord->x);
                vert_buffer_data.push_back(v.texcoord->y);
            }
        }

        for (const uint32_t index : geometry.indices) {
            index_buffer_data.push_back(index);
        }

        start_vertex += geometry.vertices.size();
        vertices_offset += geometry.vertices.size() * geometry.vertex_size;
        indices_offset += geometry.indices.size();
    }

    vk_utils::vma_buffer_handler vertex_staging_buffer;
    vk_utils::vma_buffer_handler index_staging_buffer;

    vk_utils::vma_buffer_handler vertex_buffer;
    vk_utils::vma_buffer_handler index_buffer;

    vk_utils::create_buffer(vertex_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, vert_buffer_data.size() * sizeof(float), vert_buffer_data.data());
    vk_utils::create_buffer(index_staging_buffer, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU, index_buffer_data.size() * sizeof(uint32_t), index_buffer_data.data());

    vk_utils::create_buffer(vertex_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, vert_buffer_data.size() * sizeof(float));
    vk_utils::create_buffer(index_buffer, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY, index_buffer_data.size() * sizeof(uint32_t));

    VkCommandBufferAllocateInfo cmd_buffer_alloc_info{};
    cmd_buffer_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_buffer_alloc_info.pNext = nullptr;
    cmd_buffer_alloc_info.commandPool = command_pool;
    cmd_buffer_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_buffer_alloc_info.commandBufferCount = 1;
    vk_utils::cmd_buffers_handler cmd_buffer;
    cmd_buffer.init(vk_utils::context::get().device(), command_pool, &cmd_buffer_alloc_info, 1);

    VkCommandBufferBeginInfo cmd_buffer_begin_info{};
    cmd_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_buffer_begin_info.pNext = nullptr;
    cmd_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    cmd_buffer_begin_info.pInheritanceInfo = nullptr;

    vkBeginCommandBuffer(cmd_buffer[0], &cmd_buffer_begin_info);

    VkBufferCopy vert_region{};
    vert_region.srcOffset = 0;
    vert_region.dstOffset = 0;
    vert_region.size = vert_buffer_data.size() * sizeof(float);
    vkCmdCopyBuffer(cmd_buffer[0], vertex_staging_buffer, vertex_buffer, 1, &vert_region);

    VkBufferCopy index_region{};
    index_region.srcOffset = 0;
    index_region.dstOffset = 0;
    index_region.size = index_buffer_data.size() * sizeof(uint32_t);
    vkCmdCopyBuffer(cmd_buffer[0], index_staging_buffer, index_buffer, 1, &index_region);

    VkBufferMemoryBarrier buffer_barriers[]{
        {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .pNext = nullptr,
         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = vertex_buffer,
         .offset = 0,
         .size = vert_region.size},
        {.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
         .pNext = nullptr,
         .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
         .dstAccessMask = VK_ACCESS_INDEX_READ_BIT,
         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
         .buffer = index_buffer,
         .offset = 0,
         .size = index_region.size}};

    vkCmdPipelineBarrier(
        cmd_buffer[0],
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0,
        0,
        nullptr,
        std::size(buffer_barriers),
        buffer_barriers,
        0,
        nullptr);

    vkEndCommandBuffer(cmd_buffer[0]);
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = cmd_buffer;

    auto fence = create_fence();
    vkQueueSubmit(transfer_queue, 1, &submit_info, fence);
    vkWaitForFences(vk_utils::context::get().device(), 1, fence, VK_TRUE, UINT64_MAX);

    model.vertex_buffer = std::move(vertex_buffer);
    model.index_buffer = std::move(index_buffer);

    glm::vec3 offset =  (max_pos - min_pos) / 2.0f;

    float scale_x = abs(min_pos.x - offset.x) > abs(max_pos.x - offset.x) ? abs(min_pos.x - offset.x) : abs(max_pos.x - offset.x);
    float scale_y = abs(min_pos.y - offset.y) > abs(max_pos.y - offset.y) ? abs(min_pos.y - offset.y) : abs(max_pos.y - offset.y);
    float scale_z = abs(min_pos.z - offset.z) > abs(max_pos.z - offset.z) ? abs(min_pos.z - offset.z) : abs(max_pos.z - offset.z);
    float fanal_scale = scale_x > scale_y ? scale_x : scale_y;
    fanal_scale = scale_z > fanal_scale ? 1.0f / scale_z : 1.0f / fanal_scale;

    glm::vec3 scale = glm::vec3(fanal_scale, fanal_scale, fanal_scale);
     
    offset = -(min_pos + (max_pos - min_pos) / 2.0f) * scale;

    model.model_transform = glm::translate(model.model_transform, offset);
    model.model_transform = glm::scale(model.model_transform, scale);

    RAISE_ERROR_OK();
}


ERROR_TYPE vk_utils::obj_loader::init_obj_materials(
    const std::vector<tinyobj::shape_t>& shapes,
    const std::vector<tinyobj::material_t>& materials,
    const obj_model_info& model_info,
    VkQueue transfer_queue,
    VkCommandPool command_pool,
    obj_model& model)
{
    std::vector<const char*> loaded_textures_names;
    std::vector<texture> loaded_textures;

    auto get_tex_index = [&loaded_textures_names](const char* path) {
        auto tex_it = std::find_if(loaded_textures_names.begin(), loaded_textures_names.end(), [path](const char* curr_tex_path) {
            return path != nullptr ? strcmp(curr_tex_path, path) == 0 : false;
        });
        return tex_it == loaded_textures_names.end() ? -1 : std::distance(loaded_textures_names.begin(), tex_it);
    };

    for (const auto& [tex_name, tex_val] : model_info.phong_textures) {
        for (auto tex_path : tex_val) {
            if (!tex_path.empty() && get_tex_index(tex_path.c_str()) < 0) {
                texture new_texture{};
                PASS_ERROR(load_texture_2D(tex_path.c_str(), transfer_queue, command_pool, new_texture.image, new_texture.image_view, new_texture.sampler));
                loaded_textures_names.emplace_back(tex_path.c_str());
                loaded_textures.emplace_back(std::move(new_texture));
            }
        }
    }

    auto add_material_texture = [&get_tex_index, &loaded_textures_names, &loaded_textures, &model, transfer_queue, command_pool](const std::string& image_path) {
        const auto i = get_tex_index(image_path.c_str());

        if (i >= 0) {
            return i;
        }

        loaded_textures_names.emplace_back(image_path.c_str());

        texture new_texture{};
        if (std::filesystem::path(image_path).is_absolute()) {
            HANDLE_ERROR(load_texture_2D(image_path.c_str(), transfer_queue, command_pool, new_texture.image, new_texture.image_view, new_texture.sampler));
        } else {
            auto image_abs_path = (std::filesystem::path(model.path) / image_path).string();
            HANDLE_ERROR(load_texture_2D(image_abs_path.c_str(), transfer_queue, command_pool, new_texture.image, new_texture.image_view, new_texture.sampler));
        }
        
        loaded_textures.emplace_back(std::move(new_texture));

        return static_cast<decltype(i)>(loaded_textures.size() - 1);
    };

    for (size_t i = 0; i < shapes.size(); ++i) {
        obj_phong_material curr_material{};

        const auto mesh_phong_mat_it = model_info.phong_textures.find(shapes[i].name);

        if (mesh_phong_mat_it != model_info.phong_textures.end()) {
            for (int j = 0; j < mesh_phong_mat_it->second.size(); ++j) {
                if (!mesh_phong_mat_it->second[j].empty()) {
                    curr_material.material_textures[j] = get_tex_index(mesh_phong_mat_it->second[j].c_str());
                }
            }
            model.sub_geometries[i].material = std::move(curr_material);
            continue;
        }

        auto& shape = shapes[i];

        if (shape.mesh.material_ids.front() < 0) {
            LOG_WARN("sub geometry ", shapes[i].name, " have no attached material.");
            continue;
        } else {
            auto& obj_mat = materials[shape.mesh.material_ids.front()];
            curr_material.material_textures[PHONG_DIFFUSE] = add_material_texture(obj_mat.diffuse_texname);
            curr_material.material_textures[PHONG_AMBIENT] = add_material_texture(obj_mat.ambient_texname);
            curr_material.material_textures[PHONG_SPECULAR] = add_material_texture(obj_mat.specular_texname);
            curr_material.material_textures[PHONG_SPECULAR_HIGHLIGHT] = add_material_texture(obj_mat.specular_highlight_texname);
            curr_material.material_textures[PHONG_BUMP] = add_material_texture(obj_mat.bump_texname);
            curr_material.material_textures[PHONG_DISPLACEMENT] = add_material_texture(obj_mat.displacement_texname);
            curr_material.material_textures[PHONG_ALPHA] = add_material_texture(obj_mat.alpha_texname);
            curr_material.material_textures[PHONG_REFLECTION] = add_material_texture(obj_mat.reflection_texname);
        }
    }

    model.textures = std::move(loaded_textures);

    RAISE_ERROR_OK();
}